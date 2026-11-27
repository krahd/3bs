// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace threebs {
namespace detail {
// LaunchOptions centres a dialog; it does not make the editor its owner.
// ModalComponentManager owns this callback and deletes it after dismissal.
class DialogLifetime final : private juce::ComponentListener,
                             public juce::ModalComponentManager::Callback {
public:
    DialogLifetime(juce::Component& owner, juce::DialogWindow& dialog)
        : owner_(&owner), dialog_(&dialog) { owner.addComponentListener(this); }
    ~DialogLifetime() override { detach(); }
    void modalStateFinished(int) override { detach(); }
private:
    void detach() {
        if (owner_ != nullptr) owner_->removeComponentListener(this);
        owner_ = nullptr;
        dialog_ = nullptr;
    }
    void componentBeingDeleted(juce::Component& component) override {
        component.removeComponentListener(this);
        owner_ = nullptr;
        if (dialog_ != nullptr) {
            dialog_->setVisible(false);
            dialog_->exitModalState(0);
        }
    }
    juce::Component::SafePointer<juce::Component> owner_;
    juce::Component::SafePointer<juce::DialogWindow> dialog_;
};
} // namespace detail
inline void bindDialogToOwner(juce::DialogWindow* dialog, juce::Component& owner) {
    if (dialog != nullptr)
        juce::ModalComponentManager::getInstance()->attachCallback(dialog,
            new detail::DialogLifetime(owner, *dialog));
}
} // namespace threebs
