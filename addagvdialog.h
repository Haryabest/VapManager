    #pragma once

    #include <QDialog>
    #include <QString>
    #include <functional>

    class QLineEdit;
    class QComboBox;
    class QLabel;
    class QPushButton;

    struct NewAgvData {
        QString name;
        QString serial;
        QString status;
        QString model;
        QString alias;
    };

    class AddAgvDialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit AddAgvDialog(std::function<int(int)> scale, QWidget *parent = nullptr);

        NewAgvData result;

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override;

    private:
        std::function<int(int)> s;

        QLineEdit *nameEdit;
        QLineEdit *serialEdit;
        QComboBox *statusBox;
        QComboBox *modelBox;
        QLineEdit *aliasEdit;

        QLabel *nameError;
        QLabel *serialError;
        QLabel *aliasError;

        QPushButton *addBtn;

        void validateAll();
        bool validateName();
        bool validateSerial();
        bool validateAlias();
    };
