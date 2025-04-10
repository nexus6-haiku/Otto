// ChatView.cpp
#include "ChatView.h"
#include <LayoutBuilder.h>
#include <GroupLayout.h>
#include <Catalog.h>
#include <Alert.h>
#include <Roster.h>
#include <StringView.h>
#include <SpaceLayoutItem.h>
#include <Font.h>
#include <Application.h>
#include <Path.h>
#include <Bitmap.h>
#include <Screen.h>
#include <UTF8.h>
#include <stdio.h>
#include <regex>
#include "SettingsManager.h"
#include "BFSStorage.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChatView"

// Default CSS for formatting messages
static const char* kDefaultCss = R"CSS(
body {
    font-family: "Noto Sans", Arial, sans-serif;
    font-size: 14px;
    line-height: 1.6;
    margin: 0;
    padding: 10px;
    color: #333;
    background-color: #fff;
}

.message {
    margin-bottom: 16px;
    padding: 8px 16px;
    border-radius: 8px;
    max-width: 94%;
}

.user {
    background-color: #e3f2fd;
    color: #0d47a1;
    margin-left: auto;
    margin-right: 0;
}

.assistant {
    background-color: #f1f8e9;
    color: #1b5e20;
    margin-left: 0;
    margin-right: auto;
}

.system {
    background-color: #fff3e0;
    color: #bf360c;
    font-style: italic;
    margin-left: 0;
    margin-right: auto;
}

.message-header {
    font-weight: bold;
    margin-bottom: 5px;
}

code {
    font-family: "Menlo", "Monaco", "Courier New", monospace;
    background-color: #f5f5f5;
    padding: 2px 4px;
    border-radius: 3px;
    font-size: 90%;
}

pre {
    background-color: #f5f5f5;
    padding: 10px;
    border-radius: 4px;
    overflow: auto;
    font-family: "Menlo", "Monaco", "Courier New", monospace;
    font-size: 90%;
    line-height: 1.4;
}

table {
    border-collapse: collapse;
    width: 100%;
    margin: 10px 0;
}

th, td {
    border: 1px solid #ddd;
    padding: 8px;
    text-align: left;
}

th {
    background-color: #f2f2f2;
    font-weight: bold;
}

a {
    color: #2196F3;
    text-decoration: none;
}

a:hover {
    text-decoration: underline;
}

blockquote {
    margin: 10px 0;
    padding: 10px 20px;
    border-left: 4px solid #ddd;
    background-color: #f9f9f9;
    color: #555;
}

ul, ol {
    margin-left: 20px;
}

img {
    max-width: 100%;
    height: auto;
}

.timestamp {
    font-size: 11px;
    color: #777;
    margin-top: 4px;
    text-align: right;
}
)CSS";

// Custom HTML View for rendering chat messages
class HtmlView : public BView {
public:
    HtmlView(const char* name)
        : BView(name, B_WILL_DRAW | B_FRAME_EVENTS)
    {
        // Use the system's control background color
        SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    }

    virtual void AttachedToWindow() {
        BView::AttachedToWindow();

        // Make sure we have the proper background color
        SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    }

    virtual void Draw(BRect updateRect) {
        // The parent view (ChatView) will handle all drawing
    }

    virtual void FrameResized(float width, float height) {
        // When the view is resized, invalidate to force redraw
        Invalidate();

        // Notify parent to redraw
        BView* parent = Parent();
        if (parent) {
            BMessage msg(B_VIEW_RESIZED);
            parent->MessageReceived(&msg);
        }
    }

    virtual void GetPreferredSize(float* width, float* height) {
        if (width)
            *width = 640;
        if (height)
            *height = 400;
    }
};

// HTML Container Implementation
HtmlContainer::HtmlContainer(ChatView* owner)
    : fOwner(owner),
      fView(nullptr),
      fTextColor(make_color(0, 0, 0))
{
    fFont = be_plain_font;
}

HtmlContainer::~HtmlContainer()
{
}

litehtml::uint_ptr HtmlContainer::create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm)
{
    BFont* font = new BFont(fFont);

    // Set font properties
    if (faceName && strlen(faceName) > 0) {
        font->SetFamilyAndStyle(faceName, NULL);
    }

    if (size > 0) {
        font->SetSize(size);
    }

    // Set bold/italic/etc.
    uint16 face = B_REGULAR_FACE;
    if (weight >= 700) {  // Bold is 700+ in CSS
        face |= B_BOLD_FACE;
    }
    if (italic == litehtml::font_style_italic) {
        face |= B_ITALIC_FACE;
    }

    font->SetFace(face);

    // Set underline/strikethrough if needed
    uint16 flags = 0;
    // Haiku doesn't have text decoration flags, we have to emulate them

    font->SetFlags(flags);

    // Return font metrics if requested
    if (fm) {
        font_height height;
        font->GetHeight(&height);

        fm->ascent = height.ascent;
        fm->descent = height.descent;
        fm->height = height.ascent + height.descent + height.leading;
        fm->x_height = height.ascent * 0.5; // Approximation
    }

    return (litehtml::uint_ptr)font;
}

void HtmlContainer::delete_font(litehtml::uint_ptr hFont)
{
    BFont* font = (BFont*)hFont;
    delete font;
}

int HtmlContainer::text_width(const char* text, litehtml::uint_ptr hFont)
{
    if (!fView || !text) return 0;

    BFont* font = (BFont*)hFont;
    if (!font) font = &fFont;

    float width = font->StringWidth(text);
    return (int)width;
}

void HtmlContainer::draw_text(litehtml::uint_ptr hdc, const char* text,
                             litehtml::uint_ptr hFont, litehtml::web_color color,
                             const litehtml::position& pos)
{
    if (!fView || !text) return;

    BFont* font = (BFont*)hFont;
    if (!font) font = &fFont;

    // Save current view state
    font_height fh;
    BFont oldFont;
    fView->GetFont(&oldFont);
    oldFont.GetHeight(&fh);

    // Set new font
    fView->SetFont(font);

    // Set text color
    rgb_color oldColor = fView->HighColor();
    rgb_color textColor = {color.red, color.green, color.blue, color.alpha};
    fView->SetHighColor(textColor);

    // Calculate baseline
    font_height fontHeight;
    font->GetHeight(&fontHeight);
    float baseline = pos.y + fontHeight.ascent;

    // Draw text
    fView->DrawString(text, BPoint(pos.x, baseline));

    // Restore view state
    fView->SetHighColor(oldColor);
    fView->SetFont(&oldFont);
}

int HtmlContainer::pt_to_px(int pt) const
{
    // Convert points to pixels (approximately)
    return (int)(pt * 1.3333);
}

int HtmlContainer::get_default_font_size() const
{
    return 14; // Default font size
}

const char* HtmlContainer::get_default_font_name() const
{
    static const char* defaultFont = "Noto Sans";
    return defaultFont;
}

void HtmlContainer::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker)
{
    if (!fView) return;

    BFont* font = (BFont*)marker.font;
    if (!font) font = &fFont;

    // Save current state
    font_height fh;
    BFont oldFont;
    fView->GetFont(&oldFont);
    oldFont.GetHeight(&fh);

    rgb_color oldColor = fView->HighColor();

    // Set font and color
    fView->SetFont(font);
    rgb_color color = {marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha};
    fView->SetHighColor(color);

    // Draw the marker based on type
    switch (marker.marker_type) {
        case litehtml::list_style_type_circle:
        {
            // Draw circle
            BRect rect(marker.pos.x, marker.pos.y,
                       marker.pos.x + marker.pos.width,
                       marker.pos.y + marker.pos.height);
            fView->StrokeEllipse(rect);
            break;
        }
        case litehtml::list_style_type_disc:
        {
            // Draw filled circle
            BRect rect(marker.pos.x, marker.pos.y,
                       marker.pos.x + marker.pos.width,
                       marker.pos.y + marker.pos.height);
            fView->FillEllipse(rect);
            break;
        }
        case litehtml::list_style_type_square:
        {
            // Draw filled square
            BRect rect(marker.pos.x, marker.pos.y,
                       marker.pos.x + marker.pos.width,
                       marker.pos.y + marker.pos.height);
            fView->FillRect(rect);
            break;
        }
        default:
        {
            // For numeric or other list types, render as text
            char text[32];
            switch (marker.marker_type) {
                case litehtml::list_style_type_decimal:
                    sprintf(text, "%d.", marker.index);
                    break;
                case litehtml::list_style_type_lower_alpha:
                    sprintf(text, "%c.", 'a' + ((marker.index - 1) % 26));
                    break;
                case litehtml::list_style_type_upper_alpha:
                    sprintf(text, "%c.", 'A' + ((marker.index - 1) % 26));
                    break;
                case litehtml::list_style_type_lower_roman:
                case litehtml::list_style_type_upper_roman:
                    sprintf(text, "%d.", marker.index); // Simplified: should convert to roman
                    break;
                default:
                    sprintf(text, "\u2022"); // bullet
                    break;
            }

            // Calculate baseline for text drawing
            font_height fontHeight;
            font->GetHeight(&fontHeight);
            float baseline = marker.pos.y + fontHeight.ascent;

            fView->DrawString(text, BPoint(marker.pos.x, baseline));
            break;
        }
    }

    // Restore state
    fView->SetHighColor(oldColor);
    fView->SetFont(&oldFont);
}

void HtmlContainer::load_image(const char* src, const char* baseurl, bool redraw_on_ready)
{
    // We don't need to load images for this implementation
    // If we did, we would load images asynchronously here
}

void HtmlContainer::get_image_size(const char* src, const char* baseurl, litehtml::size& sz)
{
    // We don't have images in this implementation
    sz.width = 0;
    sz.height = 0;
}

void HtmlContainer::draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg)
{
    if (!fView || bg.empty()) return;

    // Save current view state
    rgb_color oldColor = fView->HighColor();

    // Draw backgrounds in reverse order (from bottom to top)
    for (auto it = bg.rbegin(); it != bg.rend(); ++it) {
        const litehtml::background_paint& bgPaint = *it;

        // Set background color
        rgb_color bgColor = {bgPaint.color.red, bgPaint.color.green, bgPaint.color.blue, bgPaint.color.alpha};
        fView->SetHighColor(bgColor);

        // Draw background rectangle
        BRect rect(bgPaint.clip_box.x, bgPaint.clip_box.y,
                  bgPaint.clip_box.x + bgPaint.clip_box.width,
                  bgPaint.clip_box.y + bgPaint.clip_box.height);
        fView->FillRect(rect);

        // Skip image drawing completely for now to avoid null pointer issues
        // This simplifies the code but removes background images
    }

    // Restore view state
    fView->SetHighColor(oldColor);
}


void HtmlContainer::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders,
                                const litehtml::position& draw_pos, bool root)
{
    if (!fView) return;

    // Save current view state
    rgb_color oldColor = fView->HighColor();

    // Draw top border
    if (borders.top.width > 0) {
        rgb_color color = {borders.top.color.red, borders.top.color.green,
                          borders.top.color.blue, borders.top.color.alpha};
        fView->SetHighColor(color);

        BRect rect(draw_pos.x, draw_pos.y,
                  draw_pos.x + draw_pos.width,
                  draw_pos.y + borders.top.width);
        fView->FillRect(rect);
    }

    // Draw right border
    if (borders.right.width > 0) {
        rgb_color color = {borders.right.color.red, borders.right.color.green,
                          borders.right.color.blue, borders.right.color.alpha};
        fView->SetHighColor(color);

        BRect rect(draw_pos.x + draw_pos.width - borders.right.width,
                  draw_pos.y + borders.top.width,
                  draw_pos.x + draw_pos.width,
                  draw_pos.y + draw_pos.height - borders.bottom.width);
        fView->FillRect(rect);
    }

    // Draw bottom border
    if (borders.bottom.width > 0) {
        rgb_color color = {borders.bottom.color.red, borders.bottom.color.green,
                          borders.bottom.color.blue, borders.bottom.color.alpha};
        fView->SetHighColor(color);

        BRect rect(draw_pos.x,
                  draw_pos.y + draw_pos.height - borders.bottom.width,
                  draw_pos.x + draw_pos.width,
                  draw_pos.y + draw_pos.height);
        fView->FillRect(rect);
    }

    // Draw left border
    if (borders.left.width > 0) {
        rgb_color color = {borders.left.color.red, borders.left.color.green,
                          borders.left.color.blue, borders.left.color.alpha};
        fView->SetHighColor(color);

        BRect rect(draw_pos.x,
                  draw_pos.y + borders.top.width,
                  draw_pos.x + borders.left.width,
                  draw_pos.y + draw_pos.height - borders.bottom.width);
        fView->FillRect(rect);
    }

    // Restore view state
    fView->SetHighColor(oldColor);
}

void HtmlContainer::set_caption(const char* caption)
{
    // Not needed for chat view
}

void HtmlContainer::set_base_url(const char* base_url)
{
    if (base_url) {
        fBaseUrl = base_url;
    }
}

void HtmlContainer::link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el)
{
    // Not needed for basic chat view
}

void HtmlContainer::on_anchor_click(const char* url, const litehtml::element::ptr& el)
{
    // Handle link clicks - open in system browser
    if (url && strlen(url) > 0) {
        be_roster->Launch("text/html", 1, const_cast<char**>(&url));
    }
}

void HtmlContainer::set_cursor(const char* cursor)
{
    // Set cursor based on the cursor name
    // For now, we'll just use the default cursor
}

void HtmlContainer::transform_text(litehtml::string& text, litehtml::text_transform tt)
{
    if (text.empty() || tt == litehtml::text_transform_none) {
        return;
    }

    if (tt == litehtml::text_transform_capitalize) {
        bool bCapNext = true;
        for (size_t i = 0; i < text.length(); i++) {
            if (bCapNext && isalpha(text[i])) {
                text[i] = toupper(text[i]);
                bCapNext = false;
            } else if (isspace(text[i])) {
                bCapNext = true;
            }
        }
    } else if (tt == litehtml::text_transform_uppercase) {
        for (size_t i = 0; i < text.length(); i++) {
            text[i] = toupper(text[i]);
        }
    } else if (tt == litehtml::text_transform_lowercase) {
        for (size_t i = 0; i < text.length(); i++) {
            text[i] = tolower(text[i]);
        }
    }
}

void HtmlContainer::import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl)
{
    // We're not loading external CSS, just using our built-in styles
    text = "";
}

void HtmlContainer::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius)
{
    // Set clipping region
    if (fView) {
        BRect clipRect(pos.x, pos.y, pos.x + pos.width, pos.y + pos.height);
        fView->ClipToRect(clipRect);
    }
}

void HtmlContainer::del_clip()
{
    // Remove clipping
    if (fView) {
        // Reset clipping region to full view
        fView->ConstrainClippingRegion(NULL);
    }
}

void HtmlContainer::get_client_rect(litehtml::position& client) const
{
    if (fView) {
        BRect bounds = fView->Bounds();
        client.x = 0;
        client.y = 0;
        client.width = bounds.Width();
        client.height = bounds.Height();
    } else {
        client.x = 0;
        client.y = 0;
        client.width = 800;
        client.height = 600;
    }
}

litehtml::element::ptr HtmlContainer::create_element(
    const char* tag_name,
    const litehtml::string_map& attributes,
    const std::shared_ptr<litehtml::document>& doc)
{
    // Let litehtml create the default element
    return nullptr;
}

void HtmlContainer::get_media_features(litehtml::media_features& media) const
{
    // Set up default media features
    if (fView) {
        BRect bounds = fView->Bounds();
        media.width = bounds.Width();
        media.height = bounds.Height();
    } else {
        media.width = 800;
        media.height = 600;
    }

    BScreen screen;
    BRect screenRect = screen.Frame();

    media.device_width = screenRect.Width();
    media.device_height = screenRect.Height();
    media.color = 8;
    media.monochrome = 0;
    media.resolution = 96; // Default DPI
}

void HtmlContainer::get_language(litehtml::string& language, litehtml::string& culture) const
{
    language = "en";
    culture = "US";
}

// ChatView Implementation
ChatView::ChatView()
    : BView("chatView", B_WILL_DRAW)
    , fActiveChat(nullptr)
    , fActiveProvider(nullptr)
    , fActiveModel(nullptr)
    , fIsBusy(false)
    , fMessenger(this)
    , fHtmlDocument(nullptr)
    , fHtmlContainer(new HtmlContainer(this))
    , fScrollPosition(0)
    , fNeedsRender(true)
    , fCssStyles(kDefaultCss)
{
    // No initialization needed here - we'll create the document when needed

    _BuildLayout();
}

ChatView::~ChatView()
{
    delete fHtmlContainer;
}

void ChatView::_BuildLayout()
{
    // Create chat display area (HTML view with scroll bars)
    fChatDisplayView = new HtmlView("chatDisplayView");
    fChatDisplayView->SetExplicitMinSize(BSize(B_SIZE_UNSET, 500));

    fChatScrollView = new BScrollView("chatScrollView", fChatDisplayView,
        B_WILL_DRAW | B_FRAME_EVENTS, true, true);

    // Set the HTML container's view
    fHtmlContainer->SetView(fChatDisplayView);

    // Create input field (text view with scroll bars)
    fInputField = new BTextView("inputField");
    fInputField->SetWordWrap(true);
    BScrollView* inputScrollView = new BScrollView("inputScrollView",
        fInputField, B_WILL_DRAW | B_FRAME_EVENTS, true, true);

    // Create buttons
    fSendButton = new BButton("sendButton", B_TRANSLATE("Send"),
        new BMessage(MSG_SEND_MESSAGE));

    fCancelButton = new BButton("cancelButton", B_TRANSLATE("Cancel"),
        new BMessage(MSG_CANCEL_REQUEST));
    fCancelButton->SetEnabled(false);

    // Set up layout
    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .Add(fChatScrollView, 10.0)
        .AddGroup(B_HORIZONTAL)
            .Add(inputScrollView, 8.0)
            .AddGroup(B_VERTICAL, 0)
                .Add(fSendButton)
                .Add(fCancelButton)
                .AddGlue()
                .End()
            .End()
        .SetInsets(B_USE_DEFAULT_SPACING)
        .End();
}

void ChatView::AttachedToWindow()
{
    BView::AttachedToWindow();

    fSendButton->SetTarget(this);
    fCancelButton->SetTarget(this);

    // Focus the input field
    fInputField->MakeFocus(true);

    // Set background color for the display view
    if (fChatDisplayView) {
        rgb_color bgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
        fChatDisplayView->SetViewColor(bgColor);
    }
}


void ChatView::Draw(BRect updateRect)
{
    BView::Draw(updateRect);

    // Render HTML if needed
    if (fNeedsRender) {
        _SafeRenderHtml();
        fNeedsRender = false;
    }
}

void ChatView::MouseDown(BPoint where)
{
    // Pass mouse events to HTML document
    if (fHtmlDocument) {
        litehtml::position::vector redraw_boxes;
        fHtmlDocument->on_lbutton_down((int)where.x, (int)where.y, (int)where.x, (int)where.y, redraw_boxes);
        fChatDisplayView->Invalidate();
    }
}

void ChatView::MouseMoved(BPoint where, uint32 transit, const BMessage* message)
{
    // Pass mouse events to HTML document
    if (fHtmlDocument) {
        litehtml::position::vector redraw_boxes;
        fHtmlDocument->on_mouse_over((int)where.x, (int)where.y, (int)where.x, (int)where.y, redraw_boxes);
        fChatDisplayView->Invalidate();
    }
}

void ChatView::MouseUp(BPoint where)
{
    // Pass mouse events to HTML document
    if (fHtmlDocument) {
        litehtml::position::vector redraw_boxes;
        fHtmlDocument->on_lbutton_up((int)where.x, (int)where.y, (int)where.x, (int)where.y, redraw_boxes);
        fChatDisplayView->Invalidate();
    }
}

void ChatView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_SEND_MESSAGE:
            _SendMessage();
            break;

        case MSG_CANCEL_REQUEST:
            if (fActiveProvider != nullptr && fIsBusy) {
                fActiveProvider->CancelRequest();
                fIsBusy = false;
                fCancelButton->SetEnabled(false);
                fSendButton->SetEnabled(true);
            }
            break;

        case MSG_MESSAGE_RECEIVED: {
            // Handle response from the LLM
            BString content;
            if (message->FindString("content", &content) == B_OK) {
                // Create a new assistant message
                ChatMessage* reply = new ChatMessage(content, MESSAGE_ROLE_ASSISTANT);

                // Get token counts if available
                int32 inputTokens = 0, outputTokens = 0;
                message->FindInt32("input_tokens", &inputTokens);
                message->FindInt32("output_tokens", &outputTokens);
                reply->SetInputTokens(inputTokens);
                reply->SetOutputTokens(outputTokens);

                // Add to chat and display
                if (fActiveChat != nullptr) {
                    fActiveChat->AddMessage(reply);
                    fActiveChat->SetUpdatedAt(time(nullptr));

                    // Save the chat
                    BFSStorage::GetInstance()->SaveChat(fActiveChat);

                    // Save usage statistics
                    if (fActiveProvider != nullptr && fActiveModel != nullptr) {
                        BFSStorage::GetInstance()->SaveUsageStats(
                            fActiveProvider->Name(),
                            fActiveModel->Name(),
                            inputTokens,
                            outputTokens
                        );
                    }
                }

                // Ensure the display is updated
                fNeedsRender = true;
                fChatDisplayView->Invalidate();

                // Update UI state
                fIsBusy = false;
                fCancelButton->SetEnabled(false);
                fSendButton->SetEnabled(true);

                // Focus the input field again
                fInputField->MakeFocus(true);
            }
            break;
        }
		case B_VIEW_RESIZED:
			// Force redraw when view is resized
			fNeedsRender = true;
			if (fChatDisplayView) {
				fChatDisplayView->Invalidate();
			}
			break;
        default:
            BView::MessageReceived(message);
            break;
    }
}

void ChatView::SetActiveChat(Chat* chat)
{
    fActiveChat = chat;
    _DisplayChat();
}

void ChatView::SetProvider(LLMProvider* provider)
{
    fActiveProvider = provider;
}

void ChatView::SetModel(LLMModel* model)
{
    fActiveModel = model;
}

void ChatView::_DisplayChat()
{
    fNeedsRender = true;

    if (fChatDisplayView) {
        fChatDisplayView->Invalidate();
        fChatDisplayView->Window()->UpdateIfNeeded();
    }
}

void ChatView::_AppendMessageToDisplay(ChatMessage* message)
{
    if (message == nullptr) {
        return;
    }

    // Mark that we need to re-render
    fNeedsRender = true;

    if (fChatDisplayView) {
        fChatDisplayView->Invalidate();
        fChatDisplayView->Window()->UpdateIfNeeded();
    }
}

void ChatView::_RenderHtml()
{
    if (!fChatDisplayView) return;

    // Generate HTML content from chat
    BString htmlContent = _CreateHtmlContent();

    // Parse HTML and create document
    fHtmlDocument = litehtml::document::createFromString(
        htmlContent.String(),
        fHtmlContainer,
        litehtml::master_css,
        fCssStyles.String()
    );

    if (!fHtmlDocument) return;

    // Get client rect
    BRect bounds = fChatDisplayView->Bounds();

    // Render the document with error handling
    bool renderSuccess = true;
    try {
        fHtmlDocument->render((int)bounds.Width());
    } catch (...) {
        printf("Error during HTML rendering (render phase)\n");
        renderSuccess = false;
    }

    // Only attempt to draw if rendering succeeded
    if (renderSuccess) {
        try {
            fHtmlDocument->draw((litehtml::uint_ptr)fChatDisplayView, 0, 0, nullptr);
        } catch (...) {
            printf("Error during HTML rendering (draw phase)\n");
        }
    }

    // Scroll to the bottom by default
    BScrollBar* vScrollBar = fChatScrollView->ScrollBar(B_VERTICAL);
    if (vScrollBar) {
        float min, max;
        vScrollBar->GetRange(&min, &max);
        vScrollBar->SetValue(max);
    }
}



BString ChatView::_CreateHtmlContent()
{
    BString html;

    // Start HTML document
    html << "<!DOCTYPE html><html><head><style>" << fCssStyles << "</style></head><body>";

    if (fActiveChat != nullptr) {
        BObjectList<ChatMessage, true>* messages = fActiveChat->Messages();
        for (int32 i = 0; i < messages->CountItems(); i++) {
            ChatMessage* message = messages->ItemAt(i);
            if (message != nullptr) {
                // Format message based on role
                BString role;
                switch (message->Role()) {
                    case MESSAGE_ROLE_USER:
                        role = "user";
                        html << "<div class='message user'>";
                        html << "<div class='message-header'>You:</div>";
                        break;

                    case MESSAGE_ROLE_ASSISTANT:
                        role = "assistant";
                        html << "<div class='message assistant'>";
                        html << "<div class='message-header'>Assistant:</div>";
                        break;

                    case MESSAGE_ROLE_SYSTEM:
                        role = "system";
                        html << "<div class='message system'>";
                        html << "<div class='message-header'>System:</div>";
                        break;
                }

                // Format the content with markdown-style formatting
                html << _FormatMessageContent(message->Content(), message->Role());

                // Add timestamp if available
                if (message->Timestamp() > 0) {
                    time_t timestamp = message->Timestamp();
                    struct tm* timeinfo = localtime(&timestamp);
                    char timeBuffer[80];
                    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S %b %d, %Y", timeinfo);
                    html << "<div class='timestamp'>" << timeBuffer << "</div>";
                }

                html << "</div>"; // Close message div
            }
        }
    }

    // End HTML document
    html << "</body></html>";

    return html;
}

BString ChatView::_FormatMessageContent(const BString& content, MessageRole role)
{
    BString result = content;

    // Escape HTML special characters
    result.ReplaceAll("&", "&amp;");
    result.ReplaceAll("<", "&lt;");
    result.ReplaceAll(">", "&gt;");

    // Format code blocks (```code```)
    std::regex codeBlockRegex("```([^`]*?)```");
    std::string contentStr(result.String());

    contentStr = std::regex_replace(contentStr, codeBlockRegex, "<pre><code>$1</code></pre>");

    // Format inline code (`code`)
    std::regex inlineCodeRegex("`([^`]+?)`");
    contentStr = std::regex_replace(contentStr, inlineCodeRegex, "<code>$1</code>");

    // Format bold (**text**)
    std::regex boldRegex("\\*\\*([^*]+?)\\*\\*");
    contentStr = std::regex_replace(contentStr, boldRegex, "<strong>$1</strong>");

    // Format italic (*text*)
    std::regex italicRegex("\\*([^*]+?)\\*");
    contentStr = std::regex_replace(contentStr, italicRegex, "<em>$1</em>");

    // Format links
    std::regex linkRegex("\\[([^\\]]+)\\]\\(([^\\)]+)\\)");
    contentStr = std::regex_replace(contentStr, linkRegex, "<a href='$2'>$1</a>");

    // Format URLs as clickable links
    std::regex urlRegex("(https?://[^\\s]+)");
    contentStr = std::regex_replace(contentStr, urlRegex, "<a href='$1'>$1</a>");

    // Convert newlines to <br> tags
    std::regex newlineRegex("\n");
    contentStr = std::regex_replace(contentStr, newlineRegex, "<br>");

    return BString(contentStr.c_str());
}

void ChatView::_SendMessage()
{
    if (fActiveChat == nullptr || fActiveProvider == nullptr || fActiveModel == nullptr) {
        // Show error alert
        BAlert* alert = new BAlert(B_TRANSLATE("Error"),
            B_TRANSLATE("Please select a chat, provider, and model first."),
            B_TRANSLATE("OK"), nullptr, nullptr,
            B_WIDTH_AS_USUAL, B_WARNING_ALERT);
        alert->Go();
        return;
    }

    // Get message text from input field
    BString messageText = fInputField->Text();
    if (messageText.IsEmpty())
        return;

    // Create a new message
    ChatMessage* message = new ChatMessage(messageText, MESSAGE_ROLE_USER);

    // Add to chat and display
    fActiveChat->AddMessage(message);
    fActiveChat->SetUpdatedAt(time(nullptr));

    // Force display update immediately
    fNeedsRender = true;
    if (fChatDisplayView) {
        fChatDisplayView->Invalidate();
        fChatDisplayView->Window()->UpdateIfNeeded();
    }

    // Save the chat (to preserve the user's message even if app crashes)
    BFSStorage::GetInstance()->SaveChat(fActiveChat);

    // Clear input field
    fInputField->SetText("");

    // Send message to the LLM provider
    fIsBusy = true;
    fSendButton->SetEnabled(false);
    fCancelButton->SetEnabled(true);

    fActiveProvider->SendMessage(*fActiveChat->Messages(), messageText, new BMessenger(this));
}


void ChatView::_SafeRenderHtml()
{
    if (!fChatDisplayView) return;

    // Get the background color from the view's parent (the system's control color)
    rgb_color bgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
    fChatDisplayView->SetViewColor(bgColor);
    fChatDisplayView->Invalidate();

    if (fActiveChat == nullptr || fActiveChat->Messages() == nullptr ||
        fActiveChat->Messages()->CountItems() == 0) {
        return;
    }

    // Calculate total height needed for all messages
    float totalHeight = 20.0; // Start with some padding
    BRect bounds = fChatDisplayView->Bounds();
    float width = bounds.Width() * 0.9;
    float bubbleWidth = width - 20;

    // Get font heights for calculations
    font_height plainHeight;
    be_plain_font->GetHeight(&plainHeight);
    float lineHeight = plainHeight.ascent + plainHeight.descent + plainHeight.leading;

    // First pass: calculate total height
    BObjectList<ChatMessage, true>* messages = fActiveChat->Messages();
    for (int32 i = 0; i < messages->CountItems(); i++) {
        ChatMessage* message = messages->ItemAt(i);
        if (message == nullptr) continue;

        // Calculate approximate number of lines for this message
        BString content = message->Content();
        int charPerLine = (int)(bubbleWidth / be_plain_font->StringWidth("M"));
        if (charPerLine <= 0) charPerLine = 1; // Avoid division by zero

        // Count actual lines by simulating text wrapping
        int lines = 0;
        int startPos = 0;
        while (startPos < content.Length()) {
            int endPos = startPos + charPerLine;
            if (endPos > content.Length())
                endPos = content.Length();
            else {
                // Try to break at a space
                int spacePos = content.FindLast(" ", endPos);
                if (spacePos > startPos && spacePos < endPos)
                    endPos = spacePos + 1;
            }

            lines++;
            startPos = endPos;
        }

        // Add extra lines to ensure we have enough space
        lines = lines > 0 ? lines : 1;
        float messageHeight = (lines + 1) * lineHeight + 30; // +1 for title, +30 for padding

        totalHeight += messageHeight + 20; // Add bubble height + spacing
    }

    // Resize the view if needed to fit all content
    if (totalHeight > fChatDisplayView->Bounds().Height()) {
        fChatDisplayView->ResizeTo(fChatDisplayView->Bounds().Width(), totalHeight);
    }

    // Second pass: actually render the messages
    float y_pos = 20.0;

    for (int32 i = 0; i < messages->CountItems(); i++) {
        ChatMessage* message = messages->ItemAt(i);
        if (message == nullptr) continue;

        BString role;
        rgb_color bubbleBgColor;
        rgb_color textColor;

        switch (message->Role()) {
            case MESSAGE_ROLE_USER:
                role = "You:";
                bubbleBgColor = {227, 242, 253}; // Light blue
                textColor = {13, 71, 161};  // Dark blue
                break;

            case MESSAGE_ROLE_ASSISTANT:
                role = "Assistant:";
                bubbleBgColor = {241, 248, 233}; // Light green
                textColor = {27, 94, 32};  // Dark green
                break;

            case MESSAGE_ROLE_SYSTEM:
                role = "System:";
                bubbleBgColor = {255, 243, 224}; // Light orange
                textColor = {191, 54, 12};  // Dark orange
                break;
        }

        // Create a message bubble
        BFont boldFont(be_bold_font);

        // Calculate text height
        BString content = message->Content();
        int charPerLine = (int)(bubbleWidth / be_plain_font->StringWidth("M"));
        if (charPerLine <= 0) charPerLine = 1; // Avoid division by zero

        // Count actual lines by simulating text wrapping
        int lines = 0;
        int startPos = 0;
        while (startPos < content.Length()) {
            int endPos = startPos + charPerLine;
            if (endPos > content.Length())
                endPos = content.Length();
            else {
                // Try to break at a space
                int spacePos = content.FindLast(" ", endPos);
                if (spacePos > startPos && spacePos < endPos)
                    endPos = spacePos + 1;
            }

            lines++;
            startPos = endPos;
        }

        // Add extra lines to ensure we have enough space
        lines = lines > 0 ? lines : 1;
        float bubbleHeight = (lines + 1) * lineHeight + 30; // +1 for title, +30 for padding

        // Draw bubble background
        BRect bubbleRect;

        if (message->Role() == MESSAGE_ROLE_USER) {
            // Align right for user messages
            bubbleRect.Set(bounds.Width() - bubbleWidth - 10, y_pos,
                          bounds.Width() - 10, y_pos + bubbleHeight);
        } else {
            // Align left for other messages
            bubbleRect.Set(10, y_pos, 10 + bubbleWidth, y_pos + bubbleHeight);
        }

        fChatDisplayView->SetHighColor(bubbleBgColor);
        fChatDisplayView->FillRoundRect(bubbleRect, 8, 8);

        // Draw role
        fChatDisplayView->SetHighColor(textColor);
        fChatDisplayView->SetFont(&boldFont);
        fChatDisplayView->DrawString(role.String(),
                                   BPoint(bubbleRect.left + 10, bubbleRect.top + plainHeight.ascent + 5));

        // Draw content
        fChatDisplayView->SetFont(be_plain_font);
        float contentY = bubbleRect.top + plainHeight.ascent + lineHeight + 10;

        // Text wrapping
        startPos = 0;
        while (startPos < content.Length()) {
            int endPos = startPos + charPerLine;
            if (endPos > content.Length())
                endPos = content.Length();
            else {
                // Try to break at a space
                int spacePos = content.FindLast(" ", endPos);
                if (spacePos > startPos && spacePos < endPos)
                    endPos = spacePos + 1;
            }

            BString line;
            content.CopyInto(line, startPos, endPos - startPos);

            fChatDisplayView->DrawString(line.String(),
                                       BPoint(bubbleRect.left + 10, contentY));

            contentY += lineHeight;
            startPos = endPos;
        }

        // Add timestamp
        if (message->Timestamp() > 0) {
            time_t timestamp = message->Timestamp();
            struct tm* timeinfo = localtime(&timestamp);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S %b %d, %Y", timeinfo);

            BString timeStr(timeBuffer);
            float timeWidth = be_plain_font->StringWidth(timeStr.String());

            fChatDisplayView->SetHighColor(100, 100, 100); // Gray
            fChatDisplayView->DrawString(timeStr.String(),
                                       BPoint(bubbleRect.right - timeWidth - 10,
                                             bubbleRect.bottom - 5));
        }

        // Update y position for next message
        y_pos = bubbleRect.bottom + 20;
    }

    // Scroll to the bottom
    BScrollBar* vScrollBar = fChatScrollView->ScrollBar(B_VERTICAL);
    if (vScrollBar) {
        float min, max;
        vScrollBar->GetRange(&min, &max);
        vScrollBar->SetValue(max);
    }
}