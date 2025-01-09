#ifndef MATCHMETHODWORKER_H
#define MATCHMETHODWORKER_H

#include <QObject>

class LuaEditorModelItem;

class MatchMethodWorker : public QObject
{
    Q_OBJECT
public:
    MatchMethodWorker(LuaEditorModelItem* luaEditorModelItem, const QString& currentText, const QString& matchedClassName, const QString& textAfterKeyword, int cursorPosition, int mouseX, int mouseY);

    void setParameters(const QString& matchedClassName, const QString& currentText, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY);

    // Method to stop the processing
    void stopProcessing(void);

    QString getTypedAfterKeyword(void) const;
public slots:
    void process(void);

Q_SIGNALS:
    void finished();

    void signal_requestProcess();
    void signal_classNameRequired(const QString& currentText, const QString& textAfterKeyword, int cursorPosition, int mouseX, int mouseY);
private:
    LuaEditorModelItem* luaEditorModelItem;
    QString typedAfterKeyword;
    QString currentText;
    int cursorPosition;
    int mouseX;
    int mouseY;
    QString matchedClassName;
    bool isProcessing; // Flag to track processing state
    bool isStopped;    // Flag to signal processing should stop
};

#endif // MATCHMETHODWORKER_H
