#include "matchvariablesworker.h"
#include "luaeditormodelitem.h"
#include "apimodel.h"

#include <QRegularExpression>
#include <QDebug>

MatchVariablesWorker::MatchVariablesWorker(LuaEditorModelItem* luaEditorModelItem, bool forSingleton, const QString& text, int cursorPosition, int mouseX, int mouseY)
    : luaEditorModelItem(luaEditorModelItem),
    forSingleton(forSingleton),
    typed(text),
    cursorPosition(cursorPosition),
    mouseX(mouseX),
    mouseY(mouseY),
    isProcessing(false),
    isStopped(true)
{

}

void MatchVariablesWorker::setParameters(bool forSingleton, const QString& text, int cursorPos, int mouseX, int mouseY)
{
    this->forSingleton = forSingleton;
    this->typed = text;
    this->cursorPosition = cursorPos;
    this->mouseX = mouseX;
    this->mouseY = mouseY;
}

void MatchVariablesWorker::stopProcessing(void)
{
    this->isStopped = true;
}

QString MatchVariablesWorker::getTyped(void) const
{
    return this->typed;
}

void MatchVariablesWorker::process(void)
{
    // Set the flag to indicate processing has started
    this->isProcessing = true;
    this->isStopped = false; // Reset the stop flag

    if (false == this->luaEditorModelItem->hasVariablesDetected())
    {
        this->luaEditorModelItem->detectVariables();
    }

    if (!ApiModel::instance()->getIsIntellisenseShown() && this->typed.isEmpty())
    {
        // Extract text from the start up to the cursor position
        QString textBeforeCursor = this->typed.left(this->cursorPosition);

        // Check if we should stop before processing further
        if (this->isStopped)
        {
            this->isProcessing = false; // Reset the flag before exiting
            return; // Exit if stopping
        }
    }

    if (!this->typed.isEmpty())
    {
        // Ensure we check again for stop condition
        if (this->isStopped)
        {
            this->isProcessing = false; // Reset the flag before exiting
            return; // Exit if stopping
        }

        QVariantMap variablesMap = this->luaEditorModelItem->processMatchedVariables(this->forSingleton, this->typed);
        if (false == variablesMap.empty())
        {
            ApiModel::instance()->setMatchedVariables(variablesMap);
        }

        this->typed.clear();

        if (false == variablesMap.empty())
        {
            // Emit signal directly since the data structure has already been updated
            Q_EMIT ApiModel::instance()->signal_showIntelliSenseMenu("forVariable", mouseX, mouseY);
        }
    }

    // Reset the processing flag before exiting
    this->isProcessing = false;
}











