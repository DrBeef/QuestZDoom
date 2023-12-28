
package com.drbeef.questzdoom;


import static android.system.Os.setenv;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import com.drbeef.externalhapticsservice.HapticServiceClient;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

@SuppressLint("SdCardPath") public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{
	private static String manufacturer = "";

	// Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first thing.
	static
	{
		manufacturer = Build.MANUFACTURER.toLowerCase(Locale.ROOT);
		if (manufacturer.contains("oculus")) // rename oculus to meta as this will probably happen in the future anyway
		{
			manufacturer = "meta";
		}

		try
		{
			//Load manufacturer specific loader
			System.loadLibrary("openxr_loader_" + manufacturer);
			setenv("OPENXR_HMD", manufacturer, true);
		} catch (Exception e)
		{}

		System.loadLibrary( "qzdoom" );
	}

	private static final String APPLICATION = "QuestZDoom";

	private HapticServiceClient externalHapticsServiceClient = null;

	private int permissionCount = 0;
	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;

	private String commandLineParams;
	private String progdir = "/sdcard/QuestZDoom";

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;

	public void shutdown() {
		if (Build.VERSION.SDK_INT >= 21) {
			// If yes, run the fancy new function to end the app and
			//  remove it from the task list.
			finishAndRemoveTask();
		} else {
			// If not, then just end the app without removing it from
			//  the task list.
			finish();
		}
	}

	public void reload(String profile) {
		if (profile != null && !profile.isEmpty()) {
			copy_file(progdir + "/commandline_" + profile + ".txt", progdir + "/commandline.txt");
			copy_file(progdir + "/profiles/commandline_" + profile + ".txt", progdir + "/commandline.txt");
		}
		restartApplication(this);
	}

	public void haptic_event(String event, int position, int intensity, float angle, float yHeight)  {

		if (externalHapticsServiceClient.hasService()) {
			try {
				//QuestZDoom doesn't use repeating patterns - set flags to 0
				int flags = 0;
				externalHapticsServiceClient.getHapticsService().hapticEvent(APPLICATION, event, position, flags, intensity, angle, yHeight);
			}
			catch (RemoteException r)
			{
				Log.e(APPLICATION, r.toString());
			}
		}
	}

	public void haptic_stopevent(String event) {

		if (externalHapticsServiceClient.hasService()) {
			try {
				externalHapticsServiceClient.getHapticsService().hapticStopEvent(APPLICATION, event);
			}
			catch (RemoteException r)
			{
				Log.e(APPLICATION, r.toString());
			}
		}
	}

	public void haptic_enable() {

		if (externalHapticsServiceClient.hasService()) {
			try {
				externalHapticsServiceClient.getHapticsService().hapticEnable();
			}
			catch (RemoteException r)
			{
				Log.e(APPLICATION, r.toString());
			}
		}
	}

	public void haptic_disable() {

		if (externalHapticsServiceClient.hasService()) {
			try {
				externalHapticsServiceClient.getHapticsService().hapticDisable();
			}
			catch (RemoteException r)
			{
				Log.e(APPLICATION, r.toString());
			}
		}
	}


	@Override protected void onCreate( Bundle icicle )
	{
		Log.v(APPLICATION, "----------------------------------------------------------------" );
		Log.v(APPLICATION, "GLES3JNIActivity::onCreate()" );
		super.onCreate( icicle );

		mView = new SurfaceView( this );
		setContentView( mView );
		mView.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes( params );

		checkPermissionsAndInitialize();
	}

	/** Initializes the Activity only if the permission has been granted. */
	private void checkPermissionsAndInitialize() {
		// Boilerplate for checking runtime permissions in Android.
		if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			ActivityCompat.requestPermissions(this,
					new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
							Manifest.permission.WRITE_EXTERNAL_STORAGE},
					WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			// Permissions have already been granted.
			create();
		}
	}

	/** Handles the user accepting the permission. */
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
		if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
				create();
			}
			else {
				finish();
				System.exit(0);
			}
		}
	}

	private boolean isPackageInstalled(String packageName, PackageManager packageManager) {
		try {
			packageManager.getPackageGids(packageName);
			return true;
		} catch (PackageManager.NameNotFoundException e) {
			return false;
		}
	}

	public void create() {
		copy_asset(progdir, "commandline.txt", false);

		//Create all required folders
		new File(progdir, "res").mkdirs();
		new File(progdir, "mods").mkdirs();
		new File(progdir, "wads").mkdirs();
		new File(progdir, "soundfonts").mkdirs();

		copy_asset(progdir, "res/lzdoom.pk3", true);
		copy_asset(progdir, "res/lz_game_support.pk3", true);
		copy_asset(progdir, "res/lights.pk3", true);
		copy_asset(progdir, "res/brightmaps.pk3", true);

		copy_asset(progdir, "mods/Ultimate-Cheat-Menu.zip", true);
		copy_asset(progdir, "mods/laser-sight-0.5.5-vr.pk3", true);

		copy_asset(progdir + "/soundfonts", "qzdoom.sf2", false);
		copy_asset(progdir + "/fm_banks", "GENMIDI.GS.wopl", false);
		copy_asset(progdir + "/fm_banks", "gs-by-papiezak-and-sneakernets.wopn", false);

		//Read these from a file and pass through
		commandLineParams = new String("qzdoom");

		//See if user is trying to use command line params
		String commandLineFilePath = progdir + "/commandline.txt";
		GLES3JNILib.prepareEnvironment(progdir);
		if(new File(commandLineFilePath).exists()) // should exist!
		{
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader(commandLineFilePath));
				String s;
				StringBuilder sb=new StringBuilder(0);
				while ((s=br.readLine())!=null) {
					if (s.indexOf("#", 0) == 0) continue;
					sb.append(s + " ");
				}
				br.close();

				commandLineParams = new String(sb.toString());
			} catch (IOException e) {
				Log.e(APPLICATION, e.toString());
				finish();
				return;
			}
		}

		externalHapticsServiceClient = new HapticServiceClient(this, (state, desc) -> {
			Log.v(APPLICATION, "ExternalHapticsService is:" + desc);
		});

		externalHapticsServiceClient.bindService();

		//If there are no IWADS, then should exit after creating the folders
		//to allow the launcher app to do its thing, otherwise it would crash anyway
		//Check that launcher is installed too
        boolean hasIWADs = ((new File(progdir, "wads").listFiles().length) > 0);
		boolean hasLauncher = new File(progdir, "no_launcher").exists() || //Allow users to run without launcher if they _really_ want to
				isPackageInstalled("com.Baggyg.QuestZDoom_Launcher", this.getPackageManager());
		mNativeHandle = GLES3JNILib.onCreate( this, commandLineParams, hasIWADs, hasLauncher );
	}

	public void copy_asset(String path, String name, boolean force) {
		File f = new File(path + "/" + name);
		if (!f.exists() || force) {
			
			//Ensure we have an appropriate folder
			String fullname = path + "/" + name;
			String directory = fullname.substring(0, fullname.lastIndexOf("/"));
			new File(directory).mkdirs();
			_copy_asset(name, path + "/" + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();

		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);

			copy_stream(in, out);

			out.close();
			in.close();

		} catch (Exception e) {

			e.printStackTrace();
		}

	}

	public void copy_file(String path_in, String path_out)
	{
		File file_in = new File(path_in);
		if (file_in.exists()) {
			File file_out = new File(path_out);
			try (
					InputStream in = new BufferedInputStream(new FileInputStream(file_in));
					OutputStream out = new BufferedOutputStream(new FileOutputStream(file_out)))
			{
				copy_stream(in, out);
			} catch (IOException e) {
				Log.e(APPLICATION, e.toString());
			}
		}
	}

	public static void copy_stream(InputStream in, OutputStream out)
			throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}

	@Override protected void onStart()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onStart()" );
		super.onStart();

		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onStart(mNativeHandle, this);
		}
	}

	@Override protected void onResume()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onResume()" );
		super.onResume();

		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onResume(mNativeHandle);
		}
	}

	@Override protected void onPause()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onPause()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onPause(mNativeHandle);
		}
		super.onPause();
	}

	@Override protected void onStop()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onStop()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onStop(mNativeHandle);
		}
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onDestroy()" );

		if ( mSurfaceHolder != null )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
		}

		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onDestroy(mNativeHandle);
		}

		externalHapticsServiceClient.stopBinding();

		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated( SurfaceHolder holder )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceCreated()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceCreated( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceChanged()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceChanged( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceDestroyed( SurfaceHolder holder )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceDestroyed()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
			mSurfaceHolder = null;
		}
	}

	/**
	 * Working method for restarting the activity
	 * @param activity
	 */
	private void restartApplication(final @NonNull Activity activity) {
		// Systems at 29/Q and later don't allow relaunch, but System.exit(0) on
		// all supported systems will relaunch ... but by killing the process, then
		// restarting the process with the back stack intact. We must make sure that
		// the launch activity is the only thing in the back stack before exiting.
		final PackageManager pm = activity.getPackageManager();
		final Intent intent = pm.getLaunchIntentForPackage(activity.getPackageName());
		activity.finishAffinity(); // Finishes all activities.
		activity.startActivity(intent);    // Start the launch activity
		System.exit(0);    // System finishes and automatically relaunches us.
	}

	/**
	 * Working method for restarting the activity
	 */
	private void restartApplicationSecondMethod() {
		Intent intent = getIntent();
		intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
		startActivity(intent);
		Runtime.getRuntime().exit(0);
	}

}
