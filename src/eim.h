/*
 * SPDX-FileCopyrightText: 2010~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _FCITX5_CHEWING_EIM_H_
#define _FCITX5_CHEWING_EIM_H_

#include <chewing.h>
#include <cstddef>
#include <cstdint>
#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-config/option.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/trackableobject.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <string>
#include <vector>

#include "chewing_plus.h"
#include <memory>
#ifdef CHEWING_WITH_VOXKIT
#include <voxkit_public.h>
#endif

namespace fcitx {

enum class ChewingCandidateLayout {
    Vertical,
    Horizontal,
};

FCITX_CONFIG_ENUM_NAME_WITH_I18N(ChewingCandidateLayout, N_("Vertical"),
                                 N_("Horizontal"));

enum class ChewingSelectionKey {
    CSK_Digit,
    CSK_asdfghjkl,
    CSK_asdfzxcv89,
    CSK_asdfjkl789,
    CSK_aoeuhtn789,
    CSK_1234qweras,
    CSK_dstnaeo789
};

FCITX_CONFIG_ENUM_NAME(ChewingSelectionKey, "1234567890", "asdfghjkl;",
                       "asdfzxcv89", "asdfjkl789", "aoeuhtn789", "1234qweras",
                       "dstnaeo789");
FCITX_CONFIG_ENUM_I18N_ANNOTATION(ChewingSelectionKey, N_("1234567890"),
                                  N_("asdfghjkl;"), N_("asdfzxcv89"),
                                  N_("asdfjkl789"), N_("aoeuhtn789"),
                                  N_("1234qweras"), N_("dstnaeo789"));

enum class ChewingLayout {
    Default,
    Hsu,
    IBM,
    GinYieh,
    ETen,
    ETen26,
    Dvorak,
    DvorakHsu,
    DACHEN_CP26,
    HanYuPinYin,
    ThlPinYin,
    Mps2PinYin,
    Carpalx,
    Colemak,
    ColemakDH_ANSI,
    ColemakDH_ORTH
};

static constexpr const char *builtin_keymaps[] = {
    "KB_DEFAULT",        "KB_HSU",        "KB_IBM",
    "KB_GIN_YIEH",       "KB_ET",         "KB_ET26",
    "KB_DVORAK",         "KB_DVORAK_HSU", "KB_DACHEN_CP26",
    "KB_HANYU_PINYIN",   "KB_THL_PINYIN", "KB_MPS2_PINYIN",
    "KB_CARPALX",        "KB_COLEMAK",    "KB_COLEMAK_DH_ANSI",
    "KB_COLEMAK_DH_ORTH"};

FCITX_CONFIG_ENUM_NAME(ChewingLayout, "Default Keyboard", "Hsu's Keyboard",
                       "IBM Keyboard", "Gin-Yieh Keyboard", "ETen Keyboard",
                       "ETen26 Keyboard", "Dvorak Keyboard",
                       "Dvorak Keyboard with Hsu's support",
                       "DACHEN_CP26 Keyboard", "Han-Yu PinYin Keyboard",
                       "THL PinYin Keyboard", "MPS2 PinYin Keyboard",
                       "Carpalx Keyboard", "Colemak Keyboard",
                       "Colemak-DH ANSI Keyboard", "Colemak-DH Orth Keyboard");
FCITX_CONFIG_ENUM_I18N_ANNOTATION(
    ChewingLayout, N_("Default Keyboard"), N_("Hsu's Keyboard"),
    N_("IBM Keyboard"), N_("Gin-Yieh Keyboard"), N_("ETen Keyboard"),
    N_("ETen26 Keyboard"), N_("Dvorak Keyboard"),
    N_("Dvorak Keyboard with Hsu's support"), N_("DACHEN_CP26 Keyboard"),
    N_("Han-Yu PinYin Keyboard"), N_("THL PinYin Keyboard"),
    N_("MPS2 PinYin Keyboard"), N_("Carpalx Keyboard"), N_("Colemak Keyboard"),
    N_("Colemak-DH ANSI Keyboard"), N_("Colemak-DH Orth Keyboard"));

class ChewingLayoutOption : public Option<ChewingLayout> {
    using Base = Option<ChewingLayout>;

public:
    using Base::Base;

    void dumpDescription(RawConfig &config) const override {
        Base::dumpDescription(config);
        config.remove("Enum");
        for (size_t i = 0; i < supportedLayouts_.size(); i++) {
            config.setValueByPath("Enum/" + std::to_string(i),
                                  _ChewingLayout_Names[static_cast<size_t>(
                                      supportedLayouts_[i])]);
            config.setValueByPath(
                "EnumI18n/" + std::to_string(i),
                ChewingLayoutI18NAnnotation::toString(supportedLayouts_[i]));
        }
    }

private:
    static std::vector<ChewingLayout> supportedLayouts() {
        std::vector<ChewingLayout> supported = {ChewingLayout::Default};
        auto defaultNum = chewing_KBStr2Num(builtin_keymaps[0]);
        for (size_t i = 1; i < ChewingLayoutI18NAnnotation::enumLength; i++) {
            auto num = chewing_KBStr2Num(builtin_keymaps[i]);
            if (num == defaultNum) {
                continue;
            }
            supported.push_back(static_cast<ChewingLayout>(i));
        }

        return supported;
    }
    std::vector<ChewingLayout> supportedLayouts_ = supportedLayouts();
};

enum class SwitchInputMethodBehavior { Clear, CommitPreedit, CommitDefault };

FCITX_CONFIG_ENUM_NAME_WITH_I18N(SwitchInputMethodBehavior, N_("Clear"),
                                 N_("Commit current preedit"),
                                 N_("Commit default selection"))

// Program the 語音模型 button launches (set by CMake when the window is
// built; empty otherwise, and the button does nothing).
#ifndef CHEWING_PLUS_MODEL_MANAGER
#define CHEWING_PLUS_MODEL_MANAGER ""
#endif

FCITX_CONFIGURATION(
    ChewingConfig,
    OptionWithAnnotation<ChewingSelectionKey, ChewingSelectionKeyI18NAnnotation>
        SelectionKey{this, "SelectionKey", _("Selection Key"),
                     ChewingSelectionKey::CSK_Digit};
    Option<bool> selectCandidateWithArrowKey{
        this, "SelectCandidateWithArrowKey",
        _("Select candidate with arrow key"), true};
    Option<int, IntConstrain> PageSize{this, "PageSize", _("Page Size"), 10,
                                       IntConstrain(3, 10)};
    OptionWithAnnotation<ChewingCandidateLayout,
                         ChewingCandidateLayoutI18NAnnotation>
        CandidateLayout{this, "CandidateLayout", _("Candidate List Layout"),
                        ChewingCandidateLayout::Horizontal};
    Option<bool> UseKeypadAsSelectionKey{
        this, "UseKeypadAsSelection", _("Use Keypad as Selection key"), false};
    OptionWithAnnotation<SwitchInputMethodBehavior,
                         SwitchInputMethodBehaviorI18NAnnotation>
        switchInputMethodBehavior{this, "SwitchInputMethodBehavior",
                                  _("Action when switching input method"),
                                  SwitchInputMethodBehavior::CommitDefault};
    Option<bool> AddPhraseForward{this, "AddPhraseForward",
                                  _("Add Phrase Forward"), true};
    Option<bool> ChoiceBackward{this, "ChoiceBackward",
                                _("Backward phrase choice"), true};
    Option<bool> AutoShiftCursor{this, "AutoShiftCursor",
                                 _("Automatically shift cursor"), false};
    Option<bool> EasySymbolInput{this, "EasySymbolInput",
                                 _("Enable easy symbol"), false};
    Option<bool> SpaceAsSelection{this, "SpaceAsSelection",
                                  _("Space as selection key"), true};
    ChewingLayoutOption Layout{this, "Layout", _("Keyboard Layout"),
                               ChewingLayout::Default};
    // Plus features (all off by default).
    Option<bool> PlusEmoji{this, "PlusEmoji", _("Plus: Emoji suggestions"),
                           false};
    Option<bool> PlusCalculator{this, "PlusCalculator",
                                _("Plus: Calculator (=)"), false};
    Option<bool> PlusSpaceLetters{
        this, "PlusSpaceLetters",
        _("Plus: Space between Chinese and ASCII letters"), false};
    Option<bool> PlusSpaceDigits{this, "PlusSpaceDigits",
                                 _("Plus: Space between Chinese and digits"),
                                 false};
    Option<bool> PlusEmojiShortcode{
        this, "PlusEmojiShortcode",
        _("Plus: Emoji shortcodes (prefix + name, e.g. ;;smile)"), false};
    Option<std::string> PlusEmojiShortcodePrefix{
        this, "PlusEmojiShortcodePrefix",
        _("Plus: Emoji shortcode prefix (one key, or the key twice for "
          "double-tap)"),
        ";;"};
    Option<int, IntConstrain> PlusEmojiDoubleTapMs{
        this, "PlusEmojiDoubleTapMs", _("Plus: Double-tap window (ms)"), 300,
        IntConstrain(100, 1000)};
    Option<bool> PlusEnglish{this, "PlusEnglish",
                             _("Plus: English buffer mode (shortcut toggles)"),
                             false};
    Option<bool> PlusEnglishHint{
        this, "PlusEnglishHint",
        _("Plus: Hint when typed keys look like an English word"), false};
    KeyListOption PlusEnglishToggleKeys{this,
                                        "PlusEnglishToggleKeys",
                                        _("Plus: English mode shortcut"),
                                        {Key("Control+Shift+space")},
                                        KeyListConstrain()};
    KeyListOption PlusEnglishDoubleTapKeys{
        this,
        "PlusEnglishDoubleTapKeys",
        _("Plus: English mode double-tap key (modifier, e.g. Right Shift)"),
        {},
        KeyListConstrain({KeyConstrainFlag::AllowModifierOnly,
                          KeyConstrainFlag::AllowModifierLess})};
    Option<int, IntConstrain> PlusEnglishDoubleTapMs{
        this, "PlusEnglishDoubleTapMs",
        _("Plus: English mode double-tap window (ms)"), 300,
        IntConstrain(100, 1000)};
    Option<bool> PlusEnglishHintCommit{
        this, "PlusEnglishHintCommit",
        _("Plus: English shortcut fills the hinted word directly"), false};
    Option<int, IntConstrain> PlusInitialsMin{
        this, "PlusInitialsMin",
        _("Plus: Initials needed before suggesting (ㄇㄇ -> 媽媽 at 2)"), 2,
        IntConstrain(2, 6)};
    Option<bool> PlusFuzzy{
        this, "PlusFuzzy",
        _("Plus: Toneless typing (finish a syllable at the next initial)"),
        true};
    Option<bool> PlusLiveChars{
        this, "PlusLiveChars",
        _("Plus: List likely characters while a syllable is being typed"),
        true};
    Option<bool> PlusSuggestions{
        this, "PlusSuggestions",
        _("Plus: Phrase suggestions (completion and initials, Shift+1-9)"),
        false};

    // Not stored in chewing.conf: mirrors `systemctl --user is-enabled
    // voxkit` when the dialog opens and switches the unit when saved.
    Option<bool> VoiceEnabled{this, "VoiceEnabled",
                              _("Voice: enable dictation (voxkit service, "
                                "about 260 MB once a model is loaded)"),
                              false};
    Option<std::string> VoiceModel{
        this, "VoiceModel",
        _("Voice: active model id (empty keeps the daemon's choice; "
          "download, delete, and choose models in the voice model window)"),
        ""};
    Option<std::string> VoiceModelStatus{
        this, "VoiceModelStatus",
        _("Voice: daemon, download state, store path, and disk use "
          "(refreshed when this dialog opens)"),
        ""};
    // A button in the configuration dialog: opens the GTK 語音模型 window
    // (speech recognition and translation models, license consent).
    ExternalOption ModelManager{
        this, "ModelManager",
        _("Voice: model manager (download, delete, and choose speech "
          "recognition and translation models)"),
        CHEWING_PLUS_MODEL_MANAGER};);

// All composition logic lives in libchewing-plus-core (chewing_plus.h), the
// same C ABI the IBus engine and the macOS input method use. This class maps
// fcitx key events in and draws the core's view out.
//
// Chinese / English: fcitx5's own "Temporarily Toggle Input Method" key
// (AltTriggerKeys, Left Shift by default) already switches to the keyboard
// layout and shows it in the indicator, so this engine does not add its own.
class ChewingEngine final : public InputMethodEngine {
public:
    ChewingEngine(Instance *instance);
    ~ChewingEngine();
    Instance *instance() { return instance_; }
    const ChewingConfig &config() { return config_; }
    void activate(const InputMethodEntry &entry,
                  InputContextEvent &event) override;
    void deactivate(const InputMethodEntry &entry,
                    InputContextEvent &event) override;
    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
    void filterKey(const InputMethodEntry & /*entry*/,
                   KeyEvent & /*event*/) override;
    void reloadConfig() override;
    void reset(const InputMethodEntry &entry,
               InputContextEvent &event) override;
    void save() override;

    const Configuration *getConfig() const override {
        refreshVoiceStatus();
        return &config_;
    }
    void setConfig(const RawConfig &config) override {
        const bool voiceWas = voiceUnitEnabled();
        config_.load(config, true);
        populateConfig();
        if (*config_.VoiceEnabled != voiceWas) {
            setVoiceUnit(*config_.VoiceEnabled);
        }
        refreshVoiceStatus();
        applyVoiceModel();
        safeSaveAsIni(config_, "conf/chewing.conf");
    }

    void updateUI(InputContext *ic);
    void flushBuffer(InputContextEvent &event);
    void doReset(InputContextEvent &event);

    chewing_plus_session *session() { return session_.get(); }
    InputContext *currentContext() { return ic_.get(); }
    // The input method menu entry 「語音模型…」 (like the macOS menu item).
    static void openModelManager();

private:
    struct SessionDelete {
        void operator()(chewing_plus_session *s) const { chewing_plus_free(s); }
    };

    bool handleCandidateKeyEvent(const KeyEvent &keyEvent) const;
    bool englishShortcut(const KeyEvent &keyEvent, bool sensitive);
    bool pickSuggestion(const KeyEvent &keyEvent, InputContext *ic);
    void updatePreedit(InputContext *ic);

    FCITX_ADDON_DEPENDENCY_LOADER(chttrans, instance_->addonManager());
#ifdef CHEWING_WITH_VOXKIT
    FCITX_ADDON_DEPENDENCY_LOADER(voxkit, instance_->addonManager());
    std::unique_ptr<HandlerTableEntry<VoxkitRegistration>> voxkitHandle_;
    void registerVoxkit();
#endif

    void populateConfig();
    void refreshVoiceStatus() const;
    void applyVoiceModel();
    static bool voiceUnitEnabled();
    static void setVoiceUnit(bool on);
    Instance *instance_;
    SimpleAction modelsAction_;
    ChewingConfig config_;
    std::unique_ptr<chewing_plus_session, SessionDelete> session_;
    TrackableObjectReference<InputContext> ic_;
    KeyList englishToggleKeys_;
    std::vector<KeySym> englishDoubleTapSyms_;
    // X11/fcitx keycodes for the same keys (Right Shift = 62, Left Shift
    // = 50). Needed because xkb shift:both_capslock_cancel reports the
    // release keysym as Caps_Lock.
    std::vector<uint32_t> englishDoubleTapCodes_;
    // English double tap on a configurable modifier (the core only knows
    // Right Shift).
    bool tapPressed_ = false;
    int64_t tapDownMs_ = 0;
    int64_t tapUpMs_ = -1;
    bool plusCursorMoved_ = false;
};

class ChewingEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        registerDomain("fcitx5-chewing", FCITX_INSTALL_LOCALEDIR);
        return new ChewingEngine(manager->instance());
    }
};
} // namespace fcitx

#endif // _FCITX5_CHEWING_EIM_H_
