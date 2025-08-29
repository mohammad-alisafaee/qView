#include "qvmenu.h"
#include "qvapplication.h"
#include <QGraphicsDropShadowEffect>

bool QVMenu::event(QEvent *e)
{
    const bool result = QMenu::event(e);
    if (qvApp->getUseCustomMenuShadow() && (e->type() == QEvent::Polish || e->type() == QEvent::PaletteChange))
    {
        auto *shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setOffset(1);
        setGraphicsEffect(shadowEffect);
    }
    return result;
}
