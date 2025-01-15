#ifndef MATCHCLASSWORKER_H
#define MATCHCLASSWORKER_H

#include <QObject>

class LuaEditorModelItem;

class MatchClassWorker : public QObject
{
    Q_OBJECT
public:
    MatchClassWorker(LuaEditorModelItem* luaEditorModelItem, bool forConstant, bool forFunctionParameters, const QString& currentText, const QString& textAfterKeyword, int cursorPosition, int mouseX, int mouseY);

    void setParameters(bool forConstant, bool forFunctionParameters, const QString& currentText, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY);

    // Method to stop the processing
    void stopProcessing(void);
public slots:
    void process(void);

Q_SIGNALS:
    void finished();

    void signal_requestProcess();
private:
    QString handleCurrentLine(const QString& segment, bool& handleOuterSegment);

    int findUnmatchedOpenBracket(const QString& line, int cursorPos);

    bool containsDelimiterWithQuoteBefore(const QString& currentLine);

    QChar containsColonOrDotBeforeComma(const QString& text, int cursorPos, int unbalancedOpenBracketPos);

    bool handleInsideFunctionParameters(void);
private:
    LuaEditorModelItem* luaEditorModelItem;
    QString currentText;
    QString typedAfterKeyword;
    int cursorPosition;
    int oldCursorPosition;
    int mouseX;
    int mouseY;
    bool forConstant;
    QString matchedClassName;
    QString matchedMethodName;
    QString restTyped;
    QString typedInsideFunction;
    bool isProcessing; // Flag to track processing state
    bool isStopped;    // Flag to signal processing should stop
    bool forFunctionParameters;
    bool forVariable;
    bool variableFound;
};

#endif // MATCHCLASSWORKER_H
