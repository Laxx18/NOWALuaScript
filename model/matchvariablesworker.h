#ifndef MATCHMVARIABLESWORKER_H
#define MATCHMVARIABLESWORKER_H

#include <QObject>

class LuaEditorModelItem;

class MatchVariablesWorker : public QObject
{
    Q_OBJECT
public:
    MatchVariablesWorker(LuaEditorModelItem* luaEditorModelItem, bool forSingleton, const QString& text, int cursorPosition, int mouseX, int mouseY);

    void setParameters(bool forSingleton, const QString& text, int cursorPos, int mouseX, int mouseY);

    // Method to stop the processing
    void stopProcessing(void);

    QString getTyped(void) const;
public slots:
    void process(void);

Q_SIGNALS:
    void finished();

    void signal_requestProcess();
private:
    LuaEditorModelItem* luaEditorModelItem;
    QString typed;
    int cursorPosition;
    int mouseX;
    int mouseY;
    bool isProcessing; // Flag to track processing state
    bool isStopped;    // Flag to signal processing should stop
    bool forSingleton;
};

#endif // MATCHMVARIABLESWORKER_H
