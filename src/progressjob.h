// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KJob>

class ProgressController;

/// `--progress --job`: the progress as a Plasma job notification instead of a window, the way Dolphin shows a copy.
///
/// It only mirrors the controller, so the D-Bus interface stays the one a window would have; a caller adds
/// `--job` and changes nothing else. Cancelling from the notification is the window's Cancel button.
class ProgressJob : public KJob
{
    Q_OBJECT

public:
    explicit ProgressJob(ProgressController* controller, QObject* parent = nullptr);

    void start() override;

protected:
    bool doKill() override;

private:
    void updateAmounts();
    void updateDescription();
    void finish(bool succeeded);

    ProgressController* m_controller;
};
