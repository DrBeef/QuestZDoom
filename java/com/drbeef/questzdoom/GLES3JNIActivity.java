
package com.drbeef.questzdoom;


import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.os.RemoteException;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import com.drbeef.externalhapticsservice.HapticServiceClient;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

@SuppressLint("SdCardPath") public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{
	// Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first thing.
	static
	{
		System.loadLibrary( "qzdoom" );
	}

	private static final String APPLICATION = "QuestZDoom";

	private HapticServiceClient externalHapticsServiceClient = null;

	private int permissionCount = 0;
	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;

	private String commandLineParams;

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;

	public void shutdown() {
		System.exit(0);
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
				Log.v(APPLICATION, r.toString());
			}
		}
	}

	public void haptic_updateevent(String event, int intensity, float angle) {

		if (externalHapticsServiceClient.hasService()) {
			try {
				externalHapticsServiceClient.getHapticsService().hapticUpdateEvent(APPLICATION, event, intensity, angle);
			}
			catch (RemoteException r)
			{
				Log.v(APPLICATION, r.toString());
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
				Log.v(APPLICATION, r.toString());
			}
		}
	}

	public void haptic_endframe() {

		if (externalHapticsServiceClient.hasService()) {
			try {
				externalHapticsServiceClient.getHapticsService().hapticFrameTick();
			}
			catch (RemoteException r)
			{
				Log.v(APPLICATION, r.toString());
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
				Log.v(APPLICATION, r.toString());
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
				Log.v(APPLICATION, r.toString());
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
			finish();
			System.exit(0);
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

		copy_asset("/sdcard/QuestZDoom", "commandline.txt", false);

		//Create all required folders
		new File("/sdcard/QuestZDoom/res").mkdirs();
		new File("/sdcard/QuestZDoom/mods").mkdirs();
		new File("/sdcard/QuestZDoom/wads").mkdirs();
		new File("/sdcard/QuestZDoom/audiopack/snd_fluidsynth").mkdirs();

		copy_asset("/sdcard/QuestZDoom", "res/lzdoom.pk3", true);
		copy_asset("/sdcard/QuestZDoom", "res/lz_game_support.pk3", true);
		copy_asset("/sdcard/QuestZDoom", "res/lights.pk3", true);
		copy_asset("/sdcard/QuestZDoom", "res/brightmaps.pk3", true);

		copy_asset("/sdcard/QuestZDoom", "mods/Ultimate-Cheat-Menu.zip", true);
		copy_asset("/sdcard/QuestZDoom", "mods/laser-sight-0.5.5-vr.pk3", true);

		copy_asset("/sdcard/QuestZDoom/audiopack", "snd_fluidsynth/fluidsynth.sf2", false);

		//Read these from a file and pass through
		commandLineParams = new String("qzdoom");

		//See if user is trying to use command line params
		if(new File("/sdcard/QuestZDoom/commandline.txt").exists()) // should exist!
		{
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader("/sdcard/QuestZDoom/commandline.txt"));
				String s;
				StringBuilder sb=new StringBuilder(0);
				while ((s=br.readLine())!=null)
					sb.append(s + " ");
				br.close();

				commandLineParams = new String(sb.toString());
			} catch (FileNotFoundException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		externalHapticsServiceClient = new HapticServiceClient(this, (state, desc) -> {
			Log.v(APPLICATION, "ExternalHapticsService is:" + desc);
		});

		externalHapticsServiceClient.bindService();

		//If there are no IWADS, then should exit after creating the folders
		//to allow the launcher app to do its thing, otherwise it would crash anyway
		//Check that launcher is installed too
        boolean hasIWADs = ((new File("/sdcard/QuestZDoom/wads").listFiles().length) > 0);
		boolean hasLauncher = //(new File("/sdcard/QuestZDoom/no_launcher").exists()) || //Allow users to run without launcher if they _really_ want to
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

}
