#pragma once

#include "CEFInclude.h"
#include "BluTypes.h"

class UBluEye;

class BrowserClient : public CefClient, public CefLifeSpanHandler, public CefDownloadHandler, public CefDisplayHandler
{

	private:
		FScriptEvent* EventEmitter;
		FLogEvent* LogEmitter;
		FString DefaultURL;

		// For lifespan
		CefRefPtr<CefBrowser> BrowserRef;
		int BrowserId;
		bool bIsClosing;

	public:
		BrowserClient(const FString& defaultUrl)
		{
			DefaultURL = defaultUrl;
		};

		void LoadURL(const FString& newURL);

		// Getter for lifespan
		virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
		{
			return this;
		}
		//required or pdf download won't work 
		virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() override 
		{
			return this; 	
		}

		virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser, 
			CefRefPtr<CefFrame> Frame,
			CefProcessId SourceProcess, 
			CefRefPtr<CefProcessMessage> Message) override;

		virtual void OnUncaughtException(CefRefPtr<CefBrowser> Browser,
			CefRefPtr<CefFrame> Frame,
			CefRefPtr<CefV8Context> Context,
			CefRefPtr<CefV8Exception> Exception,
			CefRefPtr<CefV8StackTrace> StackTrace);

		void SetEventEmitter(FScriptEvent* Emitter);
		void SetLogEmitter(FLogEvent* Emitter);

		//CefDownloadHandler
		virtual void OnBeforeDownload(
			CefRefPtr<CefBrowser> Browser,
			CefRefPtr<CefDownloadItem> DownloadItem,
			const CefString& SuggestedName,
			CefRefPtr<CefBeforeDownloadCallback> Callback) override;

		virtual void OnDownloadUpdated(
			CefRefPtr<CefBrowser> Browser,
			CefRefPtr<CefDownloadItem> DownloadItem,
			CefRefPtr<CefDownloadItemCallback> Callback) override;

		//CefLifeSpanHandler
		virtual bool OnBeforePopup(CefRefPtr<CefBrowser> Browser,
			CefRefPtr<CefFrame> Frame,
			const CefString& TargetUrl,
			const CefString& TargetFrameName,
			WindowOpenDisposition TargetDisposition,
			bool UserGesture,
			const CefPopupFeatures& PopupFeatures,
			CefWindowInfo& WindowInfo,
			CefRefPtr<CefClient>& Client,
			CefBrowserSettings& Settings,
			CefRefPtr<CefDictionaryValue>& ExtraInfo,
			bool* NoJavascriptAccess) {
			return false;
		}

		// Lifespan methods
		void OnAfterCreated(CefRefPtr<CefBrowser> Browser) override;
		void OnBeforeClose(CefRefPtr<CefBrowser> Browser) override;

		virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> Browser,
				cef_log_severity_t Level,
				const CefString& Message,
				const CefString& Source,
				int Line) override;

		virtual void OnFullscreenModeChange(CefRefPtr< CefBrowser > Browser, bool Fullscreen) override;

		virtual void OnTitleChange(CefRefPtr< CefBrowser > Browser, const CefString& Title);

		CefRefPtr<CefBrowser> GetCEFBrowser();

		// NOTE: Must be at bottom
	public:
		IMPLEMENT_REFCOUNTING(BrowserClient);
};


