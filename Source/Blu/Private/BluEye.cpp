
#include "BluEye.h"
#include "RenderHandler.h"

FBluEyeSettings::FBluEyeSettings()
{
	FrameRate = 60.f;

	ViewSize.X = 1280;
	ViewSize.Y = 720;

	bIsTransparent = false;
	bEnableWebGL = true;
	bAudioMuted = false;
	bAutoPlayEnabled = true;
	bDebugLogTick = false;
}

UBluEye::UBluEye(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

namespace {

	// When using the Views framework this object provides the delegate
	// implementation for the CefWindow that hosts the Views-based browser.
	class SimpleWindowDelegate : public CefWindowDelegate {
	public:
		explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
			: browser_view_(browser_view) {}

		void OnWindowCreated(CefRefPtr<CefWindow> window) override {
			// Add the browser view and show the window.
			window->AddChildView(browser_view_);
			window->Show();

			// Give keyboard focus to the browser view.
			browser_view_->RequestFocus();
		}

		void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
			browser_view_ = nullptr;
		}

		bool CanClose(CefRefPtr<CefWindow> window) override {
			// Allow the window to close if the browser says it's OK.
			CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
			if (browser)
				return browser->GetHost()->TryCloseBrowser();
			return true;
		}

		CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
			return CefSize(800, 600);
		}

	private:
		CefRefPtr<CefBrowserView> browser_view_;

		IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
		DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
	};

	class SimpleBrowserViewDelegate : public CefBrowserViewDelegate,
		public CefWindowDelegate {
	public:
		SimpleBrowserViewDelegate() {}

		bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
			CefRefPtr<CefBrowserView> popup_browser_view,
			bool is_devtools) override {
			// Create a new top-level Window for the popup. It will show itself after
			// creation.
			CefWindow::CreateTopLevelWindow(
				new SimpleWindowDelegate(popup_browser_view));

			// We created the Window.
			return true;
		}

		bool IsFrameless(CefRefPtr<CefWindow> window) override {
			return true;
		}

	private:
		IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
		DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
	};

}

void UBluEye::Init()
{

	/** 
	 * We don't want this running in editor unless it's PIE
	 * If we don't check this, CEF will spawn infinite processes with widget components
	 **/

	if (GEngine)
	{
		if (GEngine->IsEditor() && !GWorld->IsPlayInEditor())
		{
			UE_LOG(LogBlu, Log, TEXT("Notice: not playing - Component Will Not Initialize"));
			return;
		}
	}
	
	if (Settings.ViewSize.X <= 0 || Settings.ViewSize.Y <= 0)
	{
		UE_LOG(LogBlu, Log, TEXT("Can't initialize when Width or Height are <= 0"));
		return;
	}

	//BrowserSettings.universal_access_from_file_urls = STATE_ENABLED;
	//BrowserSettings.file_access_from_file_urls = STATE_ENABLED;

	//BrowserSettings.web_security = STATE_DISABLED;
	//BrowserSettings.fullscreen_enabled = true;

	//Info.width = (int32)Settings.ViewSize.X;
	//Info.height = (int32)Settings.ViewSize.Y;

	// Set transparant option
	//Info.SetAsWindowless(0); //bIsTransparent

	// Figure out if we want to turn on WebGL support
	if (Settings.bEnableWebGL)
	{
		if (BluManager::CPURenderSettings)
		{
			UE_LOG(LogBlu, Error, TEXT("You have enabled WebGL for this browser, but CPU Saver is enabled in BluManager.cpp - WebGL will not work!"));
		}
		BrowserSettings.webgl = STATE_ENABLED;
	}

	//NB: this setting will change it globally for all new instances
	BluManager::AutoPlay = Settings.bAutoPlayEnabled;
	ClientHandler = new BrowserClient();

	Browser = CefBrowserView::CreateBrowserView(
		ClientHandler.get(),
		"about:blank",
		BrowserSettings,
		nullptr,
		nullptr,
		new SimpleBrowserViewDelegate());

	//Browser->GetHost()->SetWindowlessFrameRate(Settings.FrameRate);
	//Browser->GetBrowser()->GetHost()->SetAudioMuted(Settings.bAudioMuted);

	// Setup JS event emitter
	ClientHandler->SetEventEmitter(&ScriptEventEmitter);
	ClientHandler->SetLogEmitter(&LogEventEmitter);

	UE_LOG(LogBlu, Log, TEXT("Component Initialized"));
	UE_LOG(LogBlu, Log, TEXT("Loading URL: %s"), *DefaultURL);

	LoadURL(DefaultURL);

	StartEventLoop();
}

void UBluEye::ExecuteJS(const FString& Code)
{
	CefString CodeStr = *Code;
	Browser->GetBrowser()->GetMainFrame()->ExecuteJavaScript(CodeStr, "", 0);
}

void UBluEye::ExecuteJSMethodWithParams(const FString& methodName, const TArray<FString> params)
{

	// Empty param string
	FString ParamString = "(";

	// Build the param string
	for (FString param : params)
	{
		ParamString += param;
		ParamString += ",";
	}
		
	// Remove the last , it's not needed
	ParamString.RemoveFromEnd(",");
	ParamString += ");";

	// time to call the function
	ExecuteJS(methodName + ParamString);
}

void UBluEye::LoadURL(const FString& newURL)
{
	FString FinalUrl = newURL;

	//Detect chrome-devtools, and re-target them to regular devtools
	if (newURL.Contains(TEXT("chrome-devtools://devtools")))
	{
		//devtools://devtools/inspector.html?v8only=true&ws=localhost:9229
		//browser->GetHost()->ShowDevTools(info, g_handler, browserSettings, CefPoint());
		FinalUrl = FinalUrl.Replace(TEXT("chrome-devtools://devtools/bundled/inspector.html"), TEXT("devtools://devtools/inspector.html"));
	}

	// Check if we want to load a local file
	if (newURL.Contains(TEXT("blui://"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
	{

		// Get the current working directory
		FString GameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

		// We're loading a local file, so replace the proto with our game directory path
		FString LocalFile = newURL.Replace(TEXT("blui://"), *GameDir, ESearchCase::IgnoreCase);

		// Now we use the file proto
		LocalFile = FString(TEXT("file:///")) + LocalFile;

		UE_LOG(LogBlu, Log, TEXT("Load Local File: %s"), *LocalFile)

		// Load it up 
		Browser->GetBrowser()->GetMainFrame()->LoadURL(*LocalFile);

		return;

	}

	// Load as usual
	Browser->GetBrowser()->GetMainFrame()->LoadURL(*FinalUrl);

}

FString UBluEye::GetCurrentURL()
{
	return FString(Browser->GetBrowser()->GetMainFrame()->GetURL().c_str());
}

void UBluEye::SetZoom(const float Scale /*= 1*/)
{
	Browser->GetBrowser()->GetHost()->SetZoomLevel(Scale);
}

float UBluEye::GetZoom()
{
	return Browser->GetBrowser()->GetHost()->GetZoomLevel();
}

void UBluEye::DownloadFile(const FString& FileUrl)
{
	Browser->GetBrowser()->GetHost()->StartDownload(*FileUrl);
	//Todo: ensure downloading works in some way, shape or form?
}

bool UBluEye::IsBrowserLoading()
{
	return Browser->GetBrowser()->IsLoading();
}

void UBluEye::ReloadBrowser(bool IgnoreCache)
{

	if (IgnoreCache)
	{
		return Browser->GetBrowser()->ReloadIgnoreCache();
	}

	Browser->GetBrowser()->Reload();

}

void UBluEye::NavBack()
{

	if (Browser->GetBrowser()->CanGoBack())
	{
		Browser->GetBrowser()->GoBack();
	}

}

void UBluEye::NavForward()
{

	if (Browser->GetBrowser()->CanGoForward())
	{
		Browser->GetBrowser()->GoForward();
	}

}

void UBluEye::ResizeBrowser(const int32 NewWidth, const int32 NewHeight)
{

	if (NewWidth <= 0 || NewHeight <= 0)
	{
		// We can't do this, just do nothing.
		UE_LOG(LogBlu, Log, TEXT("Can't resize when one or both of the sizes are <= 0!"));
		return;
	}

	// Disable the web view while we resize
	bEnabled = false;

	// Set our new Width and Height
	Settings.ViewSize.X = NewWidth;
	Settings.ViewSize.Y = NewHeight;

	// Let the browser's host know we resized it
	Browser->GetBrowser()->GetHost()->WasResized();

	// Now we can keep going
	bEnabled = true;

	UE_LOG(LogBlu, Log, TEXT("BluEye was resized!"))
}

void UBluEye::CropWindow(const int32 Y, const int32 X, const int32 NewWidth, const int32 NewHeight)
{
	// Disable the web view while we resize
	bEnabled = false;


	// Set our new Width and Height
	Settings.ViewSize.X = NewWidth;
	Settings.ViewSize.Y = NewHeight;

	// Now we can keep going
	bEnabled = true;

	UE_LOG(LogBlu, Log, TEXT("BluEye was cropped!"))
}

UBluEye* UBluEye::SetProperties(const int32 SetWidth,
	const int32 SetHeight,
	const bool SetIsTransparent,
	const bool SetEnabled,
	const bool SetWebGL,
	const FString& SetDefaultURL)
{
	Settings.ViewSize.X = SetWidth;
	Settings.ViewSize.Y = SetHeight;

	bEnabled = SetEnabled;

	Settings.bIsTransparent = SetIsTransparent;
	Settings.bEnableWebGL = SetWebGL;

	DefaultURL = SetDefaultURL;

	return this;
}

void UBluEye::TriggerMouseMove(const FVector2D& Pos, const float Scale)
{

	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SetFocus(true);
	Browser->GetBrowser()->GetHost()->SendMouseMoveEvent(MouseEvent, false);

}

void UBluEye::TriggerLeftClick(const FVector2D& Pos, const float Scale)
{
	TriggerLeftMouseDown(Pos, Scale);
	TriggerLeftMouseUp(Pos, Scale);
}

void UBluEye::TriggerRightClick(const FVector2D& Pos, const float Scale)
{
	TriggerRightMouseDown(Pos, Scale);
	TriggerRightMouseUp(Pos, Scale);
}

void UBluEye::TriggerLeftMouseDown(const FVector2D& Pos, const float Scale)
{
	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SendMouseClickEvent(MouseEvent, MBT_LEFT, false, 1);
}

void UBluEye::TriggerRightMouseDown(const FVector2D& Pos, const float Scale)
{
	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SendMouseClickEvent(MouseEvent, MBT_RIGHT, false, 1);
}

void UBluEye::TriggerLeftMouseUp(const FVector2D& Pos, const float Scale)
{
	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SendMouseClickEvent(MouseEvent, MBT_LEFT, true, 1);
}

void UBluEye::TriggerRightMouseUp(const FVector2D& Pos, const float Scale)
{
	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SendMouseClickEvent(MouseEvent, MBT_RIGHT, true, 1);
}

void UBluEye::TriggerMouseWheel(const float MouseWheelDelta, const FVector2D& Pos, const float Scale)
{
	MouseEvent.x = Pos.X / Scale;
	MouseEvent.y = Pos.Y / Scale;

	Browser->GetBrowser()->GetHost()->SendMouseWheelEvent(MouseEvent, MouseWheelDelta * 10, MouseWheelDelta * 10);
}

void UBluEye::KeyDown(FKeyEvent InKey)
{

	ProcessKeyMods(InKey);
	ProcessKeyCode(InKey);

	KeyEvent.type = KEYEVENT_KEYDOWN;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);

}

void UBluEye::KeyUp(FKeyEvent InKey)
{

	ProcessKeyMods(InKey);
	ProcessKeyCode(InKey);

	KeyEvent.type = KEYEVENT_KEYUP;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);

}

void UBluEye::KeyPress(FKeyEvent InKey)
{

	// Simply trigger down, then up key events
	KeyDown(InKey);
	KeyUp(InKey);

}

void UBluEye::ProcessKeyCode(FKeyEvent InKey)
{
	KeyEvent.native_key_code = InKey.GetKeyCode();
	KeyEvent.windows_key_code = InKey.GetKeyCode();
}

void UBluEye::CharKeyInput(FCharacterEvent CharEvent)
{

	// Process keymods like usual
	ProcessKeyMods(CharEvent);

	// Below char input needs some special treatment, se we can't use the normal key down/up methods

#if PLATFORM_MAC
	KeyEvent.character = CharEvent.GetCharacter();
#else
    KeyEvent.windows_key_code = CharEvent.GetCharacter();
    KeyEvent.native_key_code = CharEvent.GetCharacter();
#endif
	KeyEvent.type = KEYEVENT_CHAR;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);
}

void UBluEye::CharKeyDownUp(FCharacterEvent CharEvent)
{
	// Process keymods like usual
	ProcessKeyMods(CharEvent);

	// Below char input needs some special treatment, se we can't use the normal key down/up methods

#if PLATFORM_MAC
	KeyEvent.character = CharEvent.GetCharacter();
#else
	KeyEvent.windows_key_code = CharEvent.GetCharacter();
	KeyEvent.native_key_code = CharEvent.GetCharacter();
#endif
	KeyEvent.type = KEYEVENT_KEYDOWN;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);

	KeyEvent.type = KEYEVENT_KEYUP;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);
}

void UBluEye::RawCharKeyPress(const FString CharToPress, bool isRepeat,
	bool LeftShiftDown,
	bool RightShiftDown,
	bool LeftControlDown,
	bool RightControlDown,
	bool LeftAltDown,
	bool RightAltDown,
	bool LeftCommandDown,
	bool RightCommandDown,
	bool CapsLocksOn)
{

	FModifierKeysState* KeyState = new FModifierKeysState(LeftShiftDown, RightShiftDown, LeftControlDown, 
		RightControlDown, LeftAltDown, RightAltDown, LeftCommandDown, RightCommandDown, CapsLocksOn);

	FCharacterEvent* CharEvent = new FCharacterEvent(CharToPress.GetCharArray()[0], *KeyState, 0, isRepeat);

	CharKeyInput(*CharEvent);

}

void UBluEye::SpecialKeyPress(EBluSpecialKeys Key, bool LeftShiftDown,
	bool RightShiftDown,
	bool LeftControlDown,
	bool RightControlDown,
	bool LeftAltDown,
	bool RightAltDown,
	bool LeftCommandDown,
	bool RightCommandDown,
	bool CapsLocksOn)
{

	int32 KeyValue = Key;

	KeyEvent.windows_key_code = KeyValue;
	KeyEvent.native_key_code = KeyValue;
	KeyEvent.type = KEYEVENT_KEYDOWN;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);

	KeyEvent.windows_key_code = KeyValue;
	KeyEvent.native_key_code = KeyValue;
	// bits 30 and 31 should be always 1 for WM_KEYUP
	KeyEvent.type = KEYEVENT_KEYUP;
	Browser->GetBrowser()->GetHost()->SendKeyEvent(KeyEvent);

}

void UBluEye::ProcessKeyMods(FInputEvent InKey)
{

	int Mods = 0;

	// Test alt
	if (InKey.IsAltDown())
	{
		Mods |= cef_event_flags_t::EVENTFLAG_ALT_DOWN;
	}
	else
	// Test control
	if (InKey.IsControlDown())
	{
		Mods |= cef_event_flags_t::EVENTFLAG_CONTROL_DOWN;
	} 
	else
	// Test shift
	if (InKey.IsShiftDown())
	{
		Mods |= cef_event_flags_t::EVENTFLAG_SHIFT_DOWN;
	}

	KeyEvent.modifiers = Mods;

}

void UBluEye::StartEventLoop()
{
	CefRunMessageLoop();
}

void UBluEye::CloseBrowser()
{
	BeginDestroy();
}

void UBluEye::BeginDestroy()
{
	if (Browser)
	{
		// Close up the browser
		Browser->GetBrowser()->GetHost()->SetAudioMuted(true);
		Browser->GetBrowser()->GetMainFrame()->LoadURL("about:blank");
		//browser->GetMainFrame()->Delete();
		Browser->GetBrowser()->GetHost()->CloseDevTools();
		Browser->GetBrowser()->GetHost()->CloseBrowser(true);
		Browser = nullptr;


		UE_LOG(LogBlu, Warning, TEXT("Browser Closing"));
	}

	SetFlags(RF_BeginDestroyed);
	Super::BeginDestroy();
}