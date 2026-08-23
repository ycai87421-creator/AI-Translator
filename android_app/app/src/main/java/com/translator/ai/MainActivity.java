package com.translator.ai;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.view.View;
import android.view.Window;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.util.Locale;

public class MainActivity extends Activity implements TextToSpeech.OnInitListener {

    private WebView mWebView;
    private TextToSpeech mTTS;
    private boolean mTTSInitialized = false;

    public class WebAppInterface {
        Context mContext;

        WebAppInterface(Context c) {
            mContext = c;
        }

        @JavascriptInterface
        public void speak(String text, String langName) {
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
                mTTS.speak(text, TextToSpeech.QUEUE_FLUSH, null, "UTTERANCE_ID");
            }
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        
        mTTS = new TextToSpeech(this, this);

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

        // Inject Native Android TTS Interface for 100% reliable offline pronunciation
        mWebView.addJavascriptInterface(new WebAppInterface(this), "AndroidTTS");

        mWebView.loadUrl("file:///android_asset/index.html");
    }

    @Override
    public void onInit(int status) {
        if (status == TextToSpeech.SUCCESS) {
            mTTSInitialized = true;
            mTTS.setLanguage(Locale.US);
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
            mTTS.stop();
            mTTS.shutdown();
        }
        super.onDestroy();
    }
}
