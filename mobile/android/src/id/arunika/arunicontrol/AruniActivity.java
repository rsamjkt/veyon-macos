/*
 * AruniActivity.java - main activity of AruniControl Mobile
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

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.qtproject.qt.android.bindings.QtActivity;

// Forwards "arunicontrol://pair?c=..." links (from a scanned QR code, a chat
// message or the browser) to the app, both on start and while it is running
public class AruniActivity extends QtActivity
{
	private static String s_pendingLink;

	// implemented in C++ (MobileApp); returns false while Qt isn't ready yet
	static native boolean nativeHandleLink( String link );

	@Override
	public void onCreate( Bundle savedInstanceState )
	{
		super.onCreate( savedInstanceState );
		remember( getIntent() );
	}

	@Override
	protected void onNewIntent( Intent intent )
	{
		super.onNewIntent( intent );
		setIntent( intent );
		remember( intent );
		deliverPendingLink();
	}

	private static void remember( Intent intent )
	{
		if( intent == null || Intent.ACTION_VIEW.equals( intent.getAction() ) == false )
		{
			return;
		}
		final Uri data = intent.getData();
		if( data != null && "arunicontrol".equals( data.getScheme() ) )
		{
			s_pendingLink = data.toString();
		}
	}

	// called by the C++ side once it is ready, and on new intents
	public static void deliverPendingLink()
	{
		final String link = s_pendingLink;
		if( link == null )
		{
			return;
		}
		try
		{
			if( nativeHandleLink( link ) )
			{
				s_pendingLink = null;
			}
		}
		catch( UnsatisfiedLinkError e )
		{
			// native side not loaded yet - it asks for the link when ready
		}
	}
}
