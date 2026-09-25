/*
 * NetworkHelper.java - VPN and network helpers for AruniControl Mobile
 *
 * Copyright (c) 2026 AruniControl Community
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

package id.arunika.arunicontrol;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.net.VpnService;

import com.google.mlkit.vision.barcode.common.Barcode;
import com.google.mlkit.vision.codescanner.GmsBarcodeScanner;
import com.google.mlkit.vision.codescanner.GmsBarcodeScannerOptions;
import com.google.mlkit.vision.codescanner.GmsBarcodeScanning;
import com.wireguard.android.backend.GoBackend;
import com.wireguard.android.backend.Tunnel;
import com.wireguard.config.Config;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

// Embedded WireGuard tunnel (only AruniControl's own traffic is routed through
// it via IncludedApplications) plus helpers for the official ZeroTier app.
public final class NetworkHelper
{
	private static GoBackend s_backend;
	private static final ExecutorService s_worker = Executors.newSingleThreadExecutor();

	private static final Tunnel s_tunnel = new Tunnel() {
		@Override
		public String getName() { return "arunicontrol"; }

		@Override
		public void onStateChange( Tunnel.State state )
		{
			nativeTunnelStateChanged( state == Tunnel.State.UP, null );
		}
	};

	// implemented in C++ (VpnController / GatewayManager)
	static native void nativeTunnelStateChanged( boolean up, String error );
	static native void nativeQrScanned( String code, String error );

	// Google's code scanner: its own camera UI, no camera permission for the app
	public static void scanQrCode( Context context )
	{
		try
		{
			final GmsBarcodeScannerOptions options = new GmsBarcodeScannerOptions.Builder()
				.setBarcodeFormats( Barcode.FORMAT_QR_CODE )
				.build();
			final GmsBarcodeScanner scanner = GmsBarcodeScanning.getClient( context, options );
			scanner.startScan()
				.addOnSuccessListener( barcode -> nativeQrScanned( barcode.getRawValue(), null ) )
				.addOnCanceledListener( () -> nativeQrScanned( null, null ) )
				.addOnFailureListener( e -> nativeQrScanned( null, e.getMessage() != null ? e.getMessage() : e.toString() ) );
		}
		catch( Exception e )
		{
			nativeQrScanned( null, e.toString() );
		}
	}

	private NetworkHelper() {}

	private static synchronized GoBackend backend( Context context )
	{
		if( s_backend == null )
		{
			s_backend = new GoBackend( context.getApplicationContext() );
		}
		return s_backend;
	}

	// null if the user already allowed this app to create VPN connections,
	// otherwise the system consent dialog to start for a result
	public static Intent vpnConsentIntent( Context context )
	{
		return VpnService.prepare( context );
	}

	// returns an error message for an unparsable configuration, null if valid
	public static String validateConfig( String config )
	{
		try
		{
			Config.parse( new ByteArrayInputStream( config.getBytes( StandardCharsets.UTF_8 ) ) );
			return null;
		}
		catch( Exception e )
		{
			return e.getMessage() != null ? e.getMessage() : e.toString();
		}
	}

	public static void tunnelUp( Context context, String config )
	{
		final Context app = context.getApplicationContext();
		s_worker.execute( () -> {
			try
			{
				final Config parsed = Config.parse( new ByteArrayInputStream( config.getBytes( StandardCharsets.UTF_8 ) ) );
				final Tunnel.State state = backend( app ).setState( s_tunnel, Tunnel.State.UP, parsed );
				nativeTunnelStateChanged( state == Tunnel.State.UP, null );
			}
			catch( Exception e )
			{
				nativeTunnelStateChanged( false, e.getMessage() != null ? e.getMessage() : e.toString() );
			}
		} );
	}

	public static void tunnelDown( Context context )
	{
		final Context app = context.getApplicationContext();
		s_worker.execute( () -> {
			try
			{
				backend( app ).setState( s_tunnel, Tunnel.State.DOWN, null );
				nativeTunnelStateChanged( false, null );
			}
			catch( Exception e )
			{
				nativeTunnelStateChanged( false, e.getMessage() != null ? e.getMessage() : e.toString() );
			}
		} );
	}

	public static boolean isTunnelUp( Context context )
	{
		try
		{
			return backend( context ).getState( s_tunnel ) == Tunnel.State.UP;
		}
		catch( Exception e )
		{
			return false;
		}
	}

	// true if any VPN (ours, ZeroTier, ...) currently carries the default network
	public static boolean isAnyVpnActive( Context context )
	{
		final ConnectivityManager cm = (ConnectivityManager) context.getSystemService( Context.CONNECTIVITY_SERVICE );
		if( cm == null )
		{
			return false;
		}
		for( Network network : cm.getAllNetworks() )
		{
			final NetworkCapabilities caps = cm.getNetworkCapabilities( network );
			if( caps != null && caps.hasTransport( NetworkCapabilities.TRANSPORT_VPN ) )
			{
				return true;
			}
		}
		return false;
	}

	public static boolean isAppInstalled( Context context, String packageName )
	{
		try
		{
			context.getPackageManager().getPackageInfo( packageName, 0 );
			return true;
		}
		catch( PackageManager.NameNotFoundException e )
		{
			return false;
		}
	}

	// opens an installed app, otherwise its Play Store page
	public static void openApp( Context context, String packageName )
	{
		Intent intent = context.getPackageManager().getLaunchIntentForPackage( packageName );
		if( intent == null )
		{
			intent = new Intent( Intent.ACTION_VIEW, Uri.parse( "market://details?id=" + packageName ) );
		}
		intent.addFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
		try
		{
			context.startActivity( intent );
		}
		catch( Exception e )
		{
			final Intent web = new Intent( Intent.ACTION_VIEW,
										   Uri.parse( "https://play.google.com/store/apps/details?id=" + packageName ) );
			web.addFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
			context.startActivity( web );
		}
	}
}
