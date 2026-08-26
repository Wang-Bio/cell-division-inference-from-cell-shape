#ifndef DEBUGMANAGER_H
#define DEBUGMANAGER_H

class MainWindow;

class DebugManager
{
public:
    static void generate(MainWindow *mainWindow);
    static void compareBatchEstimations(MainWindow *mainWindow);
    static void exportFeatureNames(MainWindow *mainWindow);
};

#endif // DEBUGMANAGER_H
