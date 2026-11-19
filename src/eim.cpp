/*
 * SPDX-FileCopyrightText: 2012~2012 Tai-Lin Chu <tailinchu@gmail.com>
 * SPDX-FileCopyrightText: 2012~2017 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "eim.h"

#include "voxkit_client.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addoninstance.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/statusarea.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
#include <fcitx/userinterfacemanager.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

FCITX_DEFINE_LOG_CATEGORY(chewing_log, "chewing");
#define CHEWING_DEBUG() FCITX_LOGC(chewing_log, Debug)

namespace fcitx {

namespace {

constexpr auto builtin_selectkeys = std::to_array<std::string_view>({
    "1234567890",
    "asdfghjkl;",
    "asdfzxcv89",
    "asdfjkl789",
    "aoeuhtn789",
    "1234qweras",
    "dstnaeo789",
});

static_assert(builtin_selectkeys.size() ==
                  ChewingSelectionKeyI18NAnnotation::enumLength,
              "Enum mismatch");

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string Str(const char *s) { return s ? s : ""; }

// fcitx key -> core key event. Returns false for keys the core never sees.
bool MapKey(const KeyEvent &keyEvent, chewing_plus_key_event &out) {
    std::memset(&out, 0, sizeof(out));
    out.time_ms = -1;
    // The core reads evdev-style codes 10..18 for the digit row (fcitx and
    // IBus both report X11 keycodes, which match for that row).
    out.keycode = keyEvent.rawKey().code();
    const Key &key = keyEvent.key();
    const KeyStates states = keyEvent.rawKey().states();
    if (states.test(KeyState::Shift)) {
        out.mods |= CHEWING_PLUS_MOD_SHIFT;
    }
    if (states.test(KeyState::Ctrl)) {
        out.mods |= CHEWING_PLUS_MOD_CTRL;
    }
    if (states.test(KeyState::Alt)) {
        out.mods |= CHEWING_PLUS_MOD_ALT;
    }
    if (keyEvent.isRelease()) {
        out.mods |= CHEWING_PLUS_MOD_RELEASE;
    }
    switch (key.sym()) {
    case FcitxKey_BackSpace:
        out.key = CHEWING_PLUS_KEY_BACKSPACE;
        return true;
    case FcitxKey_Tab:
        out.key = CHEWING_PLUS_KEY_TAB;
        return true;
    case FcitxKey_Return:
    case FcitxKey_KP_Enter:
        out.key = CHEWING_PLUS_KEY_RETURN;
        return true;
    case FcitxKey_Escape:
        out.key = CHEWING_PLUS_KEY_ESCAPE;
        return true;
    case FcitxKey_Delete:
        out.key = CHEWING_PLUS_KEY_DELETE;
        return true;
    case FcitxKey_Home:
        out.key = CHEWING_PLUS_KEY_HOME;
        return true;
    case FcitxKey_End:
        out.key = CHEWING_PLUS_KEY_END;
        return true;
    case FcitxKey_Left:
        out.key = CHEWING_PLUS_KEY_LEFT;
        return true;
    case FcitxKey_Right:
        out.key = CHEWING_PLUS_KEY_RIGHT;
        return true;
    case FcitxKey_Up:
        out.key = CHEWING_PLUS_KEY_UP;
        return true;
    case FcitxKey_Down:
        out.key = CHEWING_PLUS_KEY_DOWN;
        return true;
    case FcitxKey_Page_Up:
        out.key = CHEWING_PLUS_KEY_PAGE_UP;
        return true;
    case FcitxKey_Page_Down:
        out.key = CHEWING_PLUS_KEY_PAGE_DOWN;
        return true;
    case FcitxKey_Shift_L:
        out.key = CHEWING_PLUS_KEY_SHIFT_L;
        return true;
    case FcitxKey_Shift_R:
        out.key = CHEWING_PLUS_KEY_SHIFT_R;
        return true;
    case FcitxKey_space:
        out.key = CHEWING_PLUS_KEY_SPACE;
        out.ascii = ' ';
        return true;
    default:
        break;
    }
    if (key.isModifier() || keyEvent.isRelease()) {
        // Other modifiers and releases still reach the core so it can tell
        // a lone tap from a modifier in use.
        return true;
    }
    uint32_t ch = Key::keySymToUnicode(key.sym());
    if (ch > 0x20 && ch < 0x7f) {
        out.ascii = ch;
        return true;
    }
    if (key.sym() >= FcitxKey_KP_0 && key.sym() <= FcitxKey_KP_9) {
        out.ascii = '0' + (key.sym() - FcitxKey_KP_0);
        return true;
    }
    return false;
}

class ChewingCandidateWord : public CandidateWord {
public:
    ChewingCandidateWord(ChewingEngine *engine, std::string str, int index)
        : CandidateWord(Text(std::move(str))), engine_(engine), index_(index) {}

    void select(InputContext *ic) const override {
        chewing_plus_select(engine_->session(), index_);
        engine_->updateUI(ic);
    }

private:
    ChewingEngine *engine_;
    int index_;
};

// The core's candidate list: a libchewing page (with the Plus emoji), or a
// Plus panel (calculator, emoji shortcode, English).
class ChewingCandidateList : public CandidateList,
                             public PageableCandidateList,
                             public CursorMovableCandidateList,
                             public CursorModifiableCandidateList {
public:
    ChewingCandidateList(ChewingEngine *engine, InputContext *ic)
        : engine_(engine), ic_(ic) {
        setPageable(this);
        setCursorMovable(this);
        setCursorModifiable(this);
        auto *s = engine_->session();
        plusPanel_ = chewing_plus_panel(s) != CHEWING_PLUS_PANEL_NONE;
        const int n = chewing_plus_candidate_count(s);
        for (int i = 0; i < n; i++) {
            words_.emplace_back(std::make_unique<ChewingCandidateWord>(
                engine_, Str(chewing_plus_candidate(s, i)), i));
            labels_.emplace_back(Str(chewing_plus_candidate_label(s, i)));
        }
    }

    const Text &label(int idx) const override {
        if (idx < 0 || idx >= size()) {
            throw std::invalid_argument("Invalid index");
        }
        return labels_[idx];
    }
    const CandidateWord &candidate(int idx) const override {
        if (idx < 0 || idx >= size()) {
            throw std::invalid_argument("Invalid index");
        }
        return *words_[idx];
    }

    bool isPlusPanel() const { return plusPanel_; }

    int size() const override { return words_.size(); }
    int cursorIndex() const override {
        if (empty() || !*engine_->config().selectCandidateWithArrowKey) {
            return -1;
        }
        return cursor_;
    }
    CandidateLayoutHint layoutHint() const override {
        switch (*engine_->config().CandidateLayout) {
        case ChewingCandidateLayout::Horizontal:
            return CandidateLayoutHint::Horizontal;
        case ChewingCandidateLayout::Vertical:
            return CandidateLayoutHint::Vertical;
        }
        return CandidateLayoutHint::Horizontal;
    }

    // Need for paging, allow rotating pages.
    bool hasPrev() const override { return !plusPanel_; }
    bool hasNext() const override { return !plusPanel_; }
    void prev() override { paging(false); }
    void next() override { paging(true); }

    bool usedNextBefore() const override { return true; }

    void prevCandidate() override {
        if (cursor_ == 0) {
            prev();
            // We do not reset cursor to the last on purpose.
            // This way it could be easily to change the range of current word.
        } else {
            cursor_ -= 1;
            ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
        }
    }

    void nextCandidate() override {
        if (cursor_ + 1 == size()) {
            next();
            cursor_ = 0;
        } else {
            cursor_ += 1;
            ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
        }
    }

    void setCursorIndex(int cursor) override {
        if (cursor < 0 || cursor >= size()) {
            return;
        }
        cursor_ = cursor;
    }

private:
    void paging(bool next) {
        if (words_.empty() || plusPanel_) {
            return;
        }
        if (chewing_plus_page(engine_->session(), next ? 1 : 0)) {
            engine_->updateUI(ic_);
        }
    }

    ChewingEngine *engine_;
    InputContext *ic_;
    std::vector<std::unique_ptr<CandidateWord>> words_;
    std::vector<Text> labels_;
    bool plusPanel_ = false;
    int cursor_ = 0;
};

std::filesystem::path ChewingDataDir() {
    const auto &sp = StandardPaths::global();
    // libchewing >= 0.14 ships static_dict.bin instead of tsi.dat. Prefer it,
    // so a leftover 0.13 tsi.dat elsewhere on XDG_DATA_DIRS is never used
    // with a 0.14 library (which cannot read it).
    for (const char *marker :
         {"libchewing/static_dict.bin", "libchewing/tsi.dat"}) {
        std::filesystem::path dictData =
            sp.locate(StandardPathsType::Data, marker);
        if (!dictData.empty()) {
            return dictData.parent_path();
        }
    }
    return {};
}

void OnCommit(void *user, const char *utf8) {
    auto *engine = static_cast<ChewingEngine *>(user);
    auto *ic = engine->currentContext();
    if (ic && utf8 && utf8[0]) {
        ic->commitString(utf8);
    }
}

} // namespace

ChewingEngine::ChewingEngine(Instance *instance) : instance_(instance) {
    auto dir = ChewingDataDir();
    // NULL lets libchewing fall back to CHEWING_PATH and its default.
    session_.reset(
        chewing_plus_new(dir.empty() ? nullptr : dir.c_str(), nullptr));
    if (session_) {
        chewing_plus_callbacks cb;
        std::memset(&cb, 0, sizeof(cb));
        cb.commit = OnCommit;
        cb.user = this;
        chewing_plus_set_callbacks(session_.get(), &cb);
    }
    reloadConfig();
#ifdef CHEWING_WITH_VOXKIT
    registerVoxkit();
#endif
}

#ifdef CHEWING_WITH_VOXKIT
void ChewingEngine::registerVoxkit() {
    if (!voxkit()) {
        return; // module not installed or disabled: nothing to do
    }
    VoxkitRegistration r;
    r.insert = [this](InputContext *ic, const std::string &text) {
        auto *previous = ic_.get();
        ic_ = ic->watch();
        chewing_plus_insert_text(session_.get(), text.c_str());
        updateUI(ic);
        if (previous && previous != ic) {
            ic_ = previous->watch();
        }
        return true;
    };
    // The user's own Chewing phrases (longest-first, capped) bias recognition
    // toward names and jargon they actually type.
    r.vocabulary = [this]() {
        std::vector<std::string> words;
        size_t count = 0;
        char **list = chewing_plus_user_phrases(session_.get(), &count);
        for (size_t i = 0; list && i < count; i++) {
            words.emplace_back(list[i]);
        }
        chewing_plus_free_strings(list);
        return words;
    };
    voxkitHandle_ =
        voxkit()->call<IVoxkit::registerInputMethod>("chewing", std::move(r));
}
#endif

ChewingEngine::~ChewingEngine() = default;

void ChewingEngine::reloadConfig() {
    readAsIni(config_, "conf/chewing.conf");
    populateConfig();
    applyVoiceModel();
}

void ChewingEngine::refreshVoiceStatus() const {
    voxkit_client client;
    voxkit_message msg;
    char err[256] = "";
    std::string text = "voxkit is not running";
    voxkit_client_init(&client);
    if (voxkit_client_connect(&client, nullptr) == 0) {
        if (voxkit_client_model_status(&client, 1, &msg, 300, err,
                                       sizeof(err)) == 0) {
            text = std::string(msg.daemon[0] ? msg.daemon : "running") + " / " +
                   (msg.state[0] ? msg.state : "idle") + " / " + msg.store +
                   " / " + std::to_string(msg.store_bytes) +
                   " bytes / active " + msg.model;
            if (msg.message[0]) {
                text += " / ";
                text += msg.message;
            }
        } else if (err[0]) {
            text = err;
        }
        voxkit_client_close(&client);
    }
    const_cast<ChewingEngine *>(this)->config_.VoiceModelStatus.setValue(
        std::move(text));
}

void ChewingEngine::applyVoiceModel() {
    const std::string id = *config_.VoiceModel;
    voxkit_client client;
    voxkit_message msg;
    char err[128];
    if (id.empty()) {
        return;
    }
    voxkit_client_init(&client);
    if (voxkit_client_connect(&client, nullptr) != 0) {
        return;
    }
    voxkit_client_model_select(&client, 1, id.c_str(), &msg, 300, err,
                               sizeof(err));
    voxkit_client_close(&client);
}

void ChewingEngine::populateConfig() {
    auto *s = session_.get();
    if (!s) {
        return;
    }
    // fcitx5 keeps its own option names (users' chewing.conf), applied
    // through the core's settings table so every frontend shares one meaning
    // per key.
    auto set = [s](const char *key, const std::string &value) {
        chewing_plus_set_setting(s, key, value.c_str());
    };
    auto flag = [](bool on) { return std::string(on ? "1" : "0"); };
    CHEWING_DEBUG() << "Set layout to: "
                    << builtin_keymaps[static_cast<int>(*config_.Layout)];
    set("keyboard_layout", builtin_keymaps[static_cast<int>(*config_.Layout)]);
    set("selection_keys",
        std::string(
            builtin_selectkeys[static_cast<int>(*config_.SelectionKey)]));
    set("page_size", std::to_string(*config_.PageSize));
    set("add_phrase_forward", flag(*config_.AddPhraseForward));
    set("choice_backward", flag(*config_.ChoiceBackward));
    set("auto_shift_cursor", flag(*config_.AutoShiftCursor));
    set("space_as_selection", flag(*config_.SpaceAsSelection));
    set("easy_symbol", flag(*config_.EasySymbolInput));

    // Plus data: built-in files from the fcitx5 data dir, user files from
    // ~/.local/share/fcitx5-chewing. The core shares them between sessions.
    const auto &sp = StandardPaths::global();
    auto userDir = sp.userDirectory(StandardPathsType::Data) / "fcitx5-chewing";
    auto builtIn = [&sp](const char *name) {
        return sp.locate(StandardPathsType::PkgData, name).string();
    };
    const std::string emojiUser =
        (userDir / "plus-emoji-shortcodes.txt").string();
    const std::string englishUser = (userDir / "plus-english.txt").string();
    chewing_plus_set_data_paths(
        s, builtIn("data/chewing-plus-emoji-shortcodes.txt").c_str(),
        emojiUser.c_str(), builtIn("data/chewing-plus-english.txt").c_str(),
        englishUser.c_str());

    set("emoji", flag(*config_.PlusEmoji));
    set("calculator", flag(*config_.PlusCalculator));
    set("space_letters", flag(*config_.PlusSpaceLetters));
    set("space_digits", flag(*config_.PlusSpaceDigits));
    set("shortcode_prefix", *config_.PlusEmojiShortcodePrefix);
    set("shortcode_tap_ms", std::to_string(*config_.PlusEmojiDoubleTapMs));
    set("shortcode", flag(*config_.PlusEmojiShortcode));

    // English shortcuts are configurable here, so this frontend detects them
    // and calls chewing_plus_toggle_english(); the core's fixed
    // Ctrl+Shift+Space and Right Shift double tap stay off.
    englishToggleKeys_.clear();
    for (const auto &k : *config_.PlusEnglishToggleKeys) {
        if (k.isValid()) {
            englishToggleKeys_.push_back(k.normalize());
        }
    }
    englishDoubleTapSyms_.clear();
    std::string tapLabel;
    for (const auto &k : *config_.PlusEnglishDoubleTapKeys) {
        if (k.isValid() && k.isModifier()) {
            englishDoubleTapSyms_.push_back(k.sym());
            if (tapLabel.empty()) {
                tapLabel =
                    Key(k.sym()).toString(KeyStringFormat::Localized) + " ×2";
            }
        }
    }
    tapPressed_ = false;
    tapUpMs_ = -1;
    std::string combo =
        englishToggleKeys_.empty()
            ? std::string()
            : englishToggleKeys_.front().toString(KeyStringFormat::Localized);
    std::string label = !combo.empty() && !tapLabel.empty()
                            ? combo + " / " + tapLabel
                            : (combo.empty() ? tapLabel : combo);
    set("english", flag(*config_.PlusEnglish));
    set("english_hint", flag(*config_.PlusEnglishHint));
    // The hint label names fcitx5's own configurable keys.
    chewing_plus_set_english(s, *config_.PlusEnglish ? 1 : 0,
                             *config_.PlusEnglishHint ? 1 : 0, label.c_str());
    chewing_plus_set_english_combo(s, 0);
    chewing_plus_set_english_double_tap(s, 0, *config_.PlusEnglishDoubleTapMs);
    set("english_hint_commit", flag(*config_.PlusEnglishHintCommit));
    chewing_plus_set_phrases_path(
        s, builtIn("data/chewing-plus-phrases.bin").c_str());
    set("initials_min", std::to_string(*config_.PlusInitialsMin));
    set("fuzzy", flag(*config_.PlusFuzzy));
    chewing_plus_set_chars_path(s,
                                builtIn("data/chewing-plus-chars.bin").c_str());
    set("live_chars", flag(*config_.PlusLiveChars));
    set("suggestions", flag(*config_.PlusSuggestions));
    chewing_plus_reset(s);
}

void ChewingEngine::doReset(InputContextEvent &event) {
    chewing_plus_reset(session_.get());
    updateUI(event.inputContext());
}

void ChewingEngine::save() {}

void ChewingEngine::activate(const InputMethodEntry & /*entry*/,
                             InputContextEvent &event) {
    // Request chttrans.
    // Fullwidth is not required for chewing.
    chttrans();
    auto *ic = event.inputContext();
    if (auto *action =
            instance_->userInterfaceManager().lookupAction("chttrans")) {
        ic->statusArea().addAction(StatusGroup::InputMethod, action);
    }
    if (!ic_.isNull() && ic_.get() != ic) {
        doReset(event);
    }
    ic_ = ic->watch();
}

void ChewingEngine::deactivate(const InputMethodEntry &entry,
                               InputContextEvent &event) {
    if (event.type() == EventType::InputContextSwitchInputMethod) {
        flushBuffer(event);
    } else {
        reset(entry, event);
    }
}

void ChewingEngine::reset(const InputMethodEntry & /*entry*/,
                          InputContextEvent &event) {
    doReset(event);
}

bool ChewingEngine::handleCandidateKeyEvent(const KeyEvent &keyEvent) const {
    auto *ic = keyEvent.inputContext();
    auto candidateList = std::dynamic_pointer_cast<ChewingCandidateList>(
        ic->inputPanel().candidateList());
    if (!candidateList || candidateList->isPlusPanel()) {
        return false;
    }

    const KeyList keypadKeys{Key{FcitxKey_KP_1}, Key{FcitxKey_KP_2},
                             Key{FcitxKey_KP_3}, Key{FcitxKey_KP_4},
                             Key{FcitxKey_KP_5}, Key{FcitxKey_KP_6},
                             Key{FcitxKey_KP_7}, Key{FcitxKey_KP_8},
                             Key{FcitxKey_KP_9}, Key{FcitxKey_KP_0}};
    if (*config_.UseKeypadAsSelectionKey) {
        if (int index = keyEvent.key().keyListIndex(keypadKeys);
            index >= 0 && index < candidateList->size()) {
            candidateList->candidate(index).select(ic);
            return true;
        }
    }

    if (!*config_.selectCandidateWithArrowKey) {
        return false;
    }

    const bool horizontal =
        *config_.CandidateLayout == ChewingCandidateLayout::Horizontal;
    if (keyEvent.key().check(FcitxKey_Right)) {
        horizontal ? candidateList->nextCandidate() : candidateList->next();
        return true;
    }
    if (keyEvent.key().check(FcitxKey_Left)) {
        horizontal ? candidateList->prevCandidate() : candidateList->prev();
        return true;
    }
    if (keyEvent.key().check(FcitxKey_Up)) {
        horizontal ? candidateList->prev() : candidateList->prevCandidate();
        return true;
    }
    if (keyEvent.key().check(FcitxKey_Down)) {
        horizontal ? candidateList->next() : candidateList->nextCandidate();
        return true;
    }
    if ((keyEvent.key().check(FcitxKey_Return) ||
         keyEvent.key().check(FcitxKey_KP_Enter))) {
        if (int index = candidateList->cursorIndex();
            index >= 0 && index < candidateList->size()) {
            candidateList->candidate(index).select(ic);
        }
        return true;
    }
    if (keyEvent.key().check(FcitxKey_space)) {
        candidateList->next();
        return true;
    }
    return false;
}

// The configured English shortcuts: a key combination, or a modifier tapped
// twice (press and release, nothing in between, within the window).
bool ChewingEngine::englishShortcut(const KeyEvent &keyEvent, bool sensitive) {
    if (!*config_.PlusEnglish || sensitive) {
        return false;
    }
    if (!englishDoubleTapSyms_.empty()) {
        const bool watched =
            std::find(englishDoubleTapSyms_.begin(),
                      englishDoubleTapSyms_.end(),
                      keyEvent.rawKey().sym()) != englishDoubleTapSyms_.end();
        const int64_t now = NowMs();
        const int64_t window = *config_.PlusEnglishDoubleTapMs;
        if (!watched) {
            tapPressed_ = false;
            if (!keyEvent.isRelease()) {
                tapUpMs_ = -1;
            }
        } else if (!keyEvent.isRelease()) {
            tapPressed_ = true;
            tapDownMs_ = now;
        } else if (tapPressed_) {
            tapPressed_ = false;
            if (now - tapDownMs_ > window) {
                tapUpMs_ = -1;
            } else if (tapUpMs_ >= 0 && tapDownMs_ - tapUpMs_ <= window) {
                tapUpMs_ = -1;
                return chewing_plus_toggle_english(session_.get()) != 0;
            } else {
                tapUpMs_ = now;
            }
        }
    }
    return !keyEvent.isRelease() &&
           keyEvent.key().checkKeyList(englishToggleKeys_) &&
           chewing_plus_toggle_english(session_.get()) != 0;
}

// Shift+1..9 (by key position) picks a phrase suggestion while no candidate
// list or Plus panel is open.
bool ChewingEngine::pickSuggestion(const KeyEvent &keyEvent, InputContext *ic) {
    if (keyEvent.isRelease()) {
        return false;
    }
    const Key &raw = keyEvent.rawKey();
    if (raw.states() != KeyState::Shift || raw.code() < 10 || raw.code() > 18) {
        return false;
    }
    chewing_plus_view view;
    if (!chewing_plus_get_view(session_.get(), &view) ||
        view.candidate_count > 0 || view.panel != CHEWING_PLUS_PANEL_NONE) {
        return false;
    }
    const int index = raw.code() - 10;
    if (index >= view.suggestion_count) {
        return false;
    }
    chewing_plus_select_suggestion(session_.get(), index);
    updateUI(ic);
    return true;
}

void ChewingEngine::keyEvent(const InputMethodEntry & /*entry*/,
                             KeyEvent &keyEvent) {
    auto *s = session_.get();
    auto *ic = keyEvent.inputContext();
    if (!s) {
        return;
    }
    if (ic_.get() != ic) {
        ic_ = ic->watch();
    }
    const bool sensitive =
        ic->capabilityFlags().testAny(CapabilityFlag::PasswordOrSensitive);
    chewing_plus_set_sensitive(s, sensitive ? 1 : 0);

    if (englishShortcut(keyEvent, sensitive)) {
        keyEvent.filterAndAccept();
        updateUI(ic);
        return;
    }
    if (pickSuggestion(keyEvent, ic)) {
        keyEvent.filterAndAccept();
        return;
    }

    // Arrow keys move the highlight in a Plus panel; Enter then picks it.
    if (!keyEvent.isRelease() &&
        chewing_plus_panel(s) != CHEWING_PLUS_PANEL_NONE) {
        auto *list = ic->inputPanel().candidateList().get();
        const Key &key = keyEvent.key();
        if (list && (key.check(FcitxKey_Down) || key.check(FcitxKey_Up))) {
            if (auto *m = list->toCursorMovable()) {
                key.check(FcitxKey_Down) ? m->nextCandidate()
                                         : m->prevCandidate();
                plusCursorMoved_ = true;
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            }
            keyEvent.filterAndAccept();
            return;
        }
        if (list && plusCursorMoved_ &&
            (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter))) {
            int i = list->cursorIndex();
            chewing_plus_select(s, i < 0 ? 0 : i);
            updateUI(ic);
            keyEvent.filterAndAccept();
            return;
        }
    }

    if (!keyEvent.isRelease() && handleCandidateKeyEvent(keyEvent)) {
        keyEvent.filterAndAccept();
        return;
    }

    chewing_plus_key_event ev;
    if (!MapKey(keyEvent, ev)) {
        return;
    }
    const int consumed = chewing_plus_key(s, &ev);
    if (consumed) {
        keyEvent.filterAndAccept();
    }
    if (!keyEvent.isRelease() || consumed) {
        updateUI(ic);
    }
}

void ChewingEngine::filterKey(const InputMethodEntry & /*entry*/,
                              KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *ic = keyEvent.inputContext();
    if (ic->inputPanel().candidateList() &&
        (keyEvent.key().isSimple() || keyEvent.key().isCursorMove() ||
         keyEvent.key().check(FcitxKey_space, KeyState::Shift) ||
         keyEvent.key().check(FcitxKey_Tab) ||
         keyEvent.key().check(FcitxKey_Return, KeyState::Shift))) {
        keyEvent.filterAndAccept();
        return;
    }

    if (!ic->inputPanel().candidateList()) {
        // Check if this key will produce something, if so, flush
        if (!keyEvent.key().hasModifier() &&
            Key::keySymToUnicode(keyEvent.key().sym())) {
            flushBuffer(keyEvent);
        }
    }
}

void ChewingEngine::updatePreedit(InputContext *ic) {
    auto &panel = ic->inputPanel();
    panel.setClientPreedit(Text());
    panel.setPreedit(Text());
    panel.setAuxDown(Text());

    chewing_plus_view view;
    if (!chewing_plus_get_view(session_.get(), &view)) {
        return;
    }
    const std::string text = Str(view.preedit);
    const bool useClient = ic->capabilityFlags().test(CapabilityFlag::Preedit);
    const auto format =
        useClient ? TextFormatFlag::Underline : TextFormatFlag::NoFlag;
    if (!text.empty()) {
        Text preedit;
        // Text offsets are in bytes.
        const size_t hlStart = std::min(view.highlight_start, text.size());
        const size_t hlEnd =
            std::min(view.highlight_start + view.highlight_length, text.size());
        if (view.highlight_length > 0 && hlStart < hlEnd) {
            preedit.append(text.substr(0, hlStart), format);
            preedit.append(text.substr(hlStart, hlEnd - hlStart),
                           {TextFormatFlag::HighLight, format});
            preedit.append(text.substr(hlEnd), format);
        } else {
            preedit.append(text, format);
        }
        preedit.setCursor(static_cast<int>(std::min(view.cursor, text.size())));
        if (useClient) {
            panel.setClientPreedit(preedit);
        } else {
            panel.setPreedit(preedit);
        }
    }

    // Aux line: the core's hint, then phrase suggestions ("⇧1 中華民國 ...").
    std::string aux = Str(view.aux);
    for (int i = 0; i < view.suggestion_count && i < 9; i++) {
        if (!aux.empty()) {
            aux += "  ";
        }
        aux += "⇧" + std::to_string(i + 1) + " " +
               Str(chewing_plus_suggestion(session_.get(), i));
    }
    if (!aux.empty()) {
        panel.setAuxDown(Text(std::move(aux)));
    }
}

void ChewingEngine::updateUI(InputContext *ic) {
    CHEWING_DEBUG() << "updateUI";
    plusCursorMoved_ = false;
    // clean up window asap
    ic->inputPanel().reset();
    if (chewing_plus_candidate_count(session_.get()) > 0) {
        ic->inputPanel().setCandidateList(
            std::make_unique<ChewingCandidateList>(this, ic));
    }
    updatePreedit(ic);
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void ChewingEngine::flushBuffer(InputContextEvent &event) {
    int mode = CHEWING_PLUS_FLUSH_CLEAR;
    switch (*config_.switchInputMethodBehavior) {
    case SwitchInputMethodBehavior::Clear:
        mode = CHEWING_PLUS_FLUSH_CLEAR;
        break;
    case SwitchInputMethodBehavior::CommitPreedit:
        mode = CHEWING_PLUS_FLUSH_PREEDIT;
        break;
    case SwitchInputMethodBehavior::CommitDefault:
        mode = CHEWING_PLUS_FLUSH_DEFAULT;
        break;
    }
    auto *previous = ic_.get();
    ic_ = event.inputContext()->watch();
    chewing_plus_flush(session_.get(), mode);
    if (previous && previous != event.inputContext()) {
        ic_ = previous->watch();
    }
    updateUI(event.inputContext());
}

} // namespace fcitx

FCITX_ADDON_FACTORY_V2(chewing, fcitx::ChewingEngineFactory);
