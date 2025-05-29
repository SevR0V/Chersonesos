#ifndef IPLINEEDIT_H
#define IPLINEEDIT_H

#include <QLineEdit>

class IpLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit IpLineEdit(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void sanitizeInput();
    int currentSegmentIndex() const;
    void moveToSegment(int index);
    void moveToNextSegment();
    void moveToPreviousSegment();
};

#endif // IPLINEEDIT_H
