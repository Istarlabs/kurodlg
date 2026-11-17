// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "progressjob.h"
#include "progresscontroller.h"

ProgressJob::ProgressJob(ProgressController* controller, QObject* parent) : KJob(parent), m_controller(controller)
{
    setCapabilities(KJob::Killable);

    // Lives on main()'s stack; the controller ends the process, not the job.
    setAutoDelete(false);

    connect(controller, &ProgressController::valueChanged, this, &ProgressJob::updateAmounts);
    connect(controller, &ProgressController::maximumChanged, this, &ProgressJob::updateAmounts);
    connect(controller, &ProgressController::textChanged, this, &ProgressJob::updateDescription);
    connect(controller, &ProgressController::titleChanged, this, &ProgressJob::updateDescription);
    connect(controller, &ProgressController::headerChanged, this, &ProgressJob::updateDescription);

    connect(controller, &ProgressController::finishedChanged, this,
            [this]
            {
                if (m_controller->isFinished())
                {
                    finish(true);
                }
            });

    // `close` over D-Bus counts as cancelled in the controller, as it does for kdialog. For the notification
    // that is only true while there was work left: a caller that counted to the end and closed is done.
    connect(controller, &ProgressController::cancelledChanged, this,
            [this]
            {
                if (m_controller->wasCancelled())
                {
                    finish(m_controller->maximum() > 0 && m_controller->value() >= m_controller->maximum());
                }
            });
}

void ProgressJob::start()
{
    updateDescription();
    updateAmounts();
}

bool ProgressJob::doKill()
{
    // The caller sees it through wasCancelled() and decides what cancelling means, as with the window's button.
    m_controller->cancel();
    return true;
}

void ProgressJob::updateAmounts()
{
    const int maximum = m_controller->maximum();
    const int value = m_controller->value();

    // Only the percentage: the steps of a progress bar are not items, and Plasma would print "28 of 100
    // items" under the job. What the steps are - "video 2 of 5" - the caller says in the label.
    setPercent(maximum > 0 ? static_cast<unsigned long>(qBound(0, value * 100 / maximum, 100)) : 0);
}

void ProgressJob::updateDescription()
{
    // The heading names the job, as "Copying" does; the label is what it is doing right now.
    const QString heading = m_controller->header().isEmpty() ? m_controller->title() : m_controller->header();

    Q_EMIT description(this, heading);
    Q_EMIT infoMessage(this, m_controller->text());
}

void ProgressJob::finish(bool succeeded)
{
    if (isFinished())
    {
        return;
    }

    if (!succeeded)
    {
        setError(KJob::KilledJobError);
    }

    emitResult();
}
