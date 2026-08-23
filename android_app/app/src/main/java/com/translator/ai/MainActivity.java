package com.translator.ai;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.os.Build;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.util.Locale;

public class MainActivity extends Activity implements TextToSpeech.OnInitListener {

    private static final String TAG = "AITranslator";
    private WebView mWebView;
    private TextToSpeech mTTS;
    private boolean mTTSInitialized = false;
    private MediaPlayer mMediaPlayer;

    public class WebAppInterface {
        Context mContext;

        WebAppInterface(Context c) {
            mContext = c;
        }

        @JavascriptInterface
        public void playAudioUrl(final String url) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (mMediaPlayer != null) {
                            try {
                                if (mMediaPlayer.isPlaying()) {
                                    mMediaPlayer.stop();
                                }
                                mMediaPlayer.reset();
                                mMediaPlayer.release();
                            } catch (Exception ignored) {}
                            mMediaPlayer = null;
                        }

                        mMediaPlayer = new MediaPlayer();
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                            AudioAttributes attributes = new AudioAttributes.Builder()
                                    .setUsage(AudioAttributes.USAGE_MEDIA)
                                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                                    .build();
                            mMediaPlayer.setAudioAttributes(attributes);
                        } else {
                            mMediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);
                        }

                        mMediaPlayer.setDataSource(url);
                        mMediaPlayer.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
                            @Override
                            public void onPrepared(MediaPlayer mp) {
                                mp.start();
                            }
                        });
                        mMediaPlayer.setOnErrorListener(new MediaPlayer.OnErrorListener() {
                            @Override
                            public boolean onError(MediaPlayer mp, int what, int extra) {
                                Log.e(TAG, "MediaPlayer error: " + what + ", " + extra);
                                return false;
                            }
                        });
                        mMediaPlayer.prepareAsync();
                    } catch (Exception e) {
                        Log.e(TAG, "playAudioUrl failed", e);
                    }
                }
            });
        }

        @JavascriptInterface
        public void speak(final String text, final String langName) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (mTTS != null && mTTSInitialized) {
                        Locale loc = Locale.US;
                        if ("英语".equals(langName)) loc = Locale.US;
                        else if ("日语".equals(langName)) loc = Locale.JAPANESE;
                        else if ("韩语".equals(langName)) loc = Locale.KOREAN;
                        else if ("法语".equals(langName)) loc = Locale.FRENCH;
                        else if ("德语".equals(langName)) loc = Locale.GERMAN;
                        else if ("简体中文".equals(langName)) loc = Locale.CHINESE;
                        else if ("繁体中文".equals(langName)) loc = Locale.TRADITIONAL_CHINESE;

                        mTTS.setLanguage(loc);
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                            mTTS.speak(text, TextToSpeech.QUEUE_FLUSH, null, "UTTERANCE_ID");
                        } else {
                            mTTS.speak(text, TextToSpeech.QUEUE_FLUSH, null);
                        }
                    }
                }
            });
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);

        try {
            mTTS = new TextToSpeech(this, this);
        } catch (Exception e) {
            Log.e(TAG, "TTS init error", e);
        }

        mWebView = new WebView(this);
        setContentView(mWebView);

        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setAllowFileAccessFromFileURLs(true);
        settings.setAllowUniversalAccessFromFileURLs(true);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        mWebView.setWebViewClient(new WebViewClient());
        mWebView.setWebChromeClient(new WebChromeClient());
        mWebView.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);

        mWebView.addJavascriptInterface(new WebAppInterface(this), "AndroidTTS");

        mWebView.loadUrl("file:///android_asset/index.html");
    }

    @Override
    public void onInit(int status) {
        if (status == TextToSpeech.SUCCESS) {
            mTTSInitialized = true;
            if (mTTS != null) {
                mTTS.setLanguage(Locale.US);
            }
        }
    }

    @Override
    public void onBackPressed() {
        if (mWebView != null && mWebView.canGoBack()) {
            mWebView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        if (mTTS != null) {
            try {
                mTTS.stop();
                mTTS.shutdown();
            } catch (Exception ignored) {}
        }
        if (mMediaPlayer != null) {
            try {
                if (mMediaPlayer.isPlaying()) {
                    mMediaPlayer.stop();
                }
                mMediaPlayer.reset();
                mMediaPlayer.release();
            } catch (Exception ignored) {}
            mMediaPlayer = null;
        }
        super.onDestroy();
    }
}
