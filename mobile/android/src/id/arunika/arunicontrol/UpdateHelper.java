// UpdateHelper.java - hands a downloaded APK to the Android package installer
//
// Copyright (c) 2026 AruniControl Community
// This file is part of AruniControl (GPL v2, see COPYING).

package id.arunika.arunicontrol;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import androidx.core.content.FileProvider;

import java.io.File;

public class UpdateHelper
{
	// 0 = installer shown, 1 = the user has to allow installing apps first
	// (the settings page was opened), 2 = error
	public static int installApk( Context context, String path )
	{
		try {
			if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.O &&
				!context.getPackageManager().canRequestPackageInstalls() ) {
				Intent settings = new Intent( Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
											  Uri.parse( "package:" + context.getPackageName() ) );
				settings.addFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
				context.startActivity( settings );
				return 1;
			}

			// Qt's own FileProvider shares the app's files directory
			Uri uri = FileProvider.getUriForFile( context, context.getPackageName() + ".qtprovider", new File( path ) );
			Intent intent = new Intent( Intent.ACTION_VIEW );
			intent.setDataAndType( uri, "application/vnd.android.package-archive" );
			intent.addFlags( Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK );
			context.startActivity( intent );
			return 0;
		} catch( Exception e ) {
			return 2;
		}
	}
}
