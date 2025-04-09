// ChatView.h
#ifndef CHAT_VIEW_H
#define CHAT_VIEW_H

#include <View.h>
#include <TextView.h>
#include <ScrollView.h>
#include <Button.h>
#include <ObjectList.h>
#include <String.h>

#include "ChatMessage.h"
#include "ModelSelector.h"
#include "LLMProvider.h"

#include <litehtml/litehtml.h>
#include <litehtml/html.h>
#include <litehtml/document.h>
#include <litehtml/stylesheet.h>

const uint32 MSG_SEND_MESSAGE = 'send';
const uint32 MSG_MESSAGE_RECEIVED = 'rcvd';
const uint32 MSG_CANCEL_REQUEST = 'cncl';

class HtmlContainer;

class ChatView : public BView {
public:
    ChatView();
    virtual ~ChatView();
    
    virtual void AttachedToWindow();
    virtual void MessageReceived(BMessage* message);
    virtual void Draw(BRect updateRect);
    virtual void MouseDown(BPoint where);
    virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* message);
    virtual void MouseUp(BPoint where);
    
    void SetActiveChat(Chat* chat);
    void SetProvider(LLMProvider* provider);
    void SetModel(LLMModel* model);
    
    bool IsBusy() const { return fIsBusy; }
    
private:
    void _BuildLayout();
    void _DisplayChat();
    void _SendMessage();
    void _AppendMessageToDisplay(ChatMessage* message);
    void _RenderHtml();
    BString _CreateHtmlContent();
    BString _FormatMessageContent(const BString& content, MessageRole role);
    
    // UI components
    BScrollView* fChatScrollView;
    BView* fChatDisplayView;
    BTextView* fInputField;
    BButton* fSendButton;
    BButton* fCancelButton;
    
    // Data
    Chat* fActiveChat;
    LLMProvider* fActiveProvider;
    LLMModel* fActiveModel;
    bool fIsBusy;
    BMessenger fMessenger;
    
    // HTML rendering components
    litehtml::document::ptr fHtmlDocument;
    HtmlContainer* fHtmlContainer;
    float fScrollPosition;
    bool fNeedsRender;
    
    // CSS styles for rendering
    BString fCssStyles;
};

// HTML Container implementation for litehtml
class HtmlContainer : public litehtml::document_container {
public:
    HtmlContainer(ChatView* owner);
    virtual ~HtmlContainer();
    
    // Mandatory overrides from litehtml::document_container
    litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    int text_width(const char* text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc, const char* text, 
                  litehtml::uint_ptr hFont, litehtml::web_color color,
                  const litehtml::position& pos) override;
    int pt_to_px(int pt) const override;
    int get_default_font_size() const override;
    const char* get_default_font_name() const override;
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
    void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
    void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;
    void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, 
                     const litehtml::position& draw_pos, bool root) override;
    void set_caption(const char* caption) override;
    void set_base_url(const char* base_url) override;
    void link(const std::shared_ptr<litehtml::document>& doc, 
             const litehtml::element::ptr& el) override;
    void on_anchor_click(const char* url, 
                        const litehtml::element::ptr& el) override;
    void set_cursor(const char* cursor) override;
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override;
    void import_css(litehtml::string& text, const litehtml::string& url, 
                   litehtml::string& baseurl) override;
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    void del_clip() override;
    void get_client_rect(litehtml::position& client) const override;
    litehtml::element::ptr create_element(
        const char* tag_name,
        const litehtml::string_map& attributes,
        const std::shared_ptr<litehtml::document>& doc) override;
    void get_media_features(litehtml::media_features& media) const override;
    void get_language(litehtml::string& language, litehtml::string& culture) const override;
    
    // Haiku-specific methods
    void SetView(BView* view) { fView = view; }
    void SetFont(BFont font) { fFont = font; }
    
private:
    ChatView* fOwner;
    BView* fView;
    BFont fFont;
    rgb_color fTextColor;
    BString fBaseUrl;
};

#endif // CHAT_VIEW_H