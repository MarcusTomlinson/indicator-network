#include "qdbusmenuitem.h"
#include "dbusmenuconst.h"

#include <libdbusmenu-glib/client.h>

#include <QVariant>
#include <QDebug>

#define DBUSMENU_MENUITEM_WIDGET_TYPE "x-tablet-widget"

ItemList QDBusMenuItem::m_globalItemList;
static QList<QByteArray> mapedProperties;
static QList<QByteArray> typeRelatedProperties;


static QVariant GVariantToQVariant(GVariant *value)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
        return QVariant(g_variant_get_boolean(value) == TRUE);
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
        return QVariant(g_variant_get_byte(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16))
        return QVariant(g_variant_get_int16(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
        return QVariant(g_variant_get_uint16(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
        return QVariant(g_variant_get_int32(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        return QVariant(g_variant_get_uint32(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
        return QVariant((qlonglong) g_variant_get_int64(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
        return QVariant((qulonglong) g_variant_get_uint64(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
        return QVariant(g_variant_get_double(value));
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        gsize size = 0;
        const gchar *v = g_variant_get_string(value, &size);
        return QVariant(QByteArray(v, size));
    }

    qWarning() << "Unhandle variant type" << g_variant_get_type_string(value);
    return QVariant();
}

void QDBusMenuItem::onItemPropertyChanged(DbusmenuMenuitem *mi, gchar *property, GVariant *value, gpointer *data)
{
    QDBusMenuItem *self = reinterpret_cast<QDBusMenuItem*>(data);
    if (self->updateProperty(property, GVariantToQVariant(value))) {
        if (typeRelatedProperties.contains(property)) {
            self->updateType();
        }
        Q_EMIT self->changed();
    }
}

void QDBusMenuItem::onChildAdded(DbusmenuMenuitem *mi, DbusmenuMenuitem *child, guint position, gpointer *data)
{
    QDBusMenuItem *self = reinterpret_cast<QDBusMenuItem*>(data);
    new QDBusMenuItem(child, self);
}

void QDBusMenuItem::onChildRemoved(DbusmenuMenuitem *mi, DbusmenuMenuitem *child, gpointer *data)
{
    QDBusMenuItem *self = reinterpret_cast<QDBusMenuItem*>(data);
    delete self->getChild(child);
}

void QDBusMenuItem::onChildMoved(DbusmenuMenuitem *mi, DbusmenuMenuitem *child, guint newpos, guint oldpos, gpointer *data)
{
    int id = dbusmenu_menuitem_get_id(child);
    Q_ASSERT(QDBusMenuItem::m_globalItemList.contains(id));
    QDBusMenuItem *item = qobject_cast<QDBusMenuItem*>(QDBusMenuItem::m_globalItemList[id]);
    Q_ASSERT(item);
    Q_EMIT item->moved(newpos, oldpos);
}

QDBusMenuItem::QDBusMenuItem(DbusmenuMenuitem *gitem, QObject *parent)
    : QObject(parent),
      m_gitem(gitem)
{
    /*
      Only those properties are current supported
    */
    if (mapedProperties.size() == 0) {
        mapedProperties << DBUSMENU_MENUITEM_PROP_LABEL
                        << DBUSMENU_MENUITEM_PROP_ICON_NAME
                        << DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE
                        << DBUSMENU_MENUITEM_PROP_TOGGLE_STATE
                        << DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY
                        << DBUSMENU_MENUITEM_PROP_TYPE;
     }

    /*
      Those properties are related with Object type because of that,
      is necessary update the object type if one of those properties change;
    */
    if (typeRelatedProperties.size() == 0) {
        typeRelatedProperties << DBUSMENU_MENUITEM_PROP_TYPE
                              << DBUSMENU_MENUITEM_WIDGET_TYPE
                              << DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE
                              << DBUSMENU_MENUITEM_TOGGLE_CHECK
                              << DBUSMENU_MENUITEM_TOGGLE_RADIO;
    }

    loadChildren();
    loadProperties();

    g_signal_connect(G_OBJECT(m_gitem), DBUSMENU_MENUITEM_SIGNAL_PROPERTY_CHANGED, G_CALLBACK(onItemPropertyChanged), this);
    g_signal_connect(G_OBJECT(m_gitem), DBUSMENU_MENUITEM_SIGNAL_CHILD_ADDED, G_CALLBACK(onChildAdded), this);
    g_signal_connect(G_OBJECT(m_gitem), DBUSMENU_MENUITEM_SIGNAL_CHILD_REMOVED, G_CALLBACK(onChildRemoved), this);
    g_signal_connect(G_OBJECT(m_gitem), DBUSMENU_MENUITEM_SIGNAL_CHILD_MOVED, G_CALLBACK(onChildMoved), this);

    // Regiter itself on global list
    m_globalItemList[dbusmenu_menuitem_get_id(m_gitem)] = this;
}

QDBusMenuItem::~QDBusMenuItem()
{
    if (m_gitem) {
        m_globalItemList.remove(dbusmenu_menuitem_get_id(m_gitem));
        m_gitem = 0;
    }
}

DbusmenuMenuitem *QDBusMenuItem::item() const
{
    return m_gitem;
}

int QDBusMenuItem::position() const
{
    if (m_gitem) {
        DbusmenuMenuitem *parent = dbusmenu_menuitem_get_parent(m_gitem);
        return dbusmenu_menuitem_get_position(m_gitem, parent);
    }
}

QByteArray QDBusMenuItem::type() const
{
    return m_type;
}

void QDBusMenuItem::updateType()
{
    m_type = "";

    if (dbusmenu_menuitem_property_exist(m_gitem, DBUSMENU_MENUITEM_PROP_TYPE)) {
        GVariant *var = dbusmenu_menuitem_property_get_variant(m_gitem, DBUSMENU_MENUITEM_PROP_TYPE);
        QByteArray typeName = GVariantToQVariant(var).toByteArray();
        if (typeName == "x-system-settings") {
            if (dbusmenu_menuitem_property_exist(m_gitem, DBUSMENU_MENUITEM_WIDGET_TYPE)) {
                var = dbusmenu_menuitem_property_get_variant(m_gitem, DBUSMENU_MENUITEM_WIDGET_TYPE);
                m_type = GVariantToQVariant(var).toByteArray();
            } else {
                return;
            }
        } else if (typeName == "standard") {
            m_type = "unity.widgets.systemsettings.tablet.label";
        } else if (typeName == "separator") {
            m_type = "unity.widgets.systemsettings.tablet.separator";
        }

        qDebug() << "Type discovered" << m_type;
        Q_EMIT typeDiscovered();
    }
}

void QDBusMenuItem::loadChildren()
{
    GList *children = dbusmenu_menuitem_get_children(m_gitem);
    while(children != 0) {
        DbusmenuMenuitem *i = reinterpret_cast<DbusmenuMenuitem *>(children->data);
        if (i)
            new QDBusMenuItem(i, this);
        children = g_list_next(children);
    }
}

void QDBusMenuItem::loadProperties()
{
    /* parse the relevant properties */
    setProperty(DBUSMENU_PROPERTY_ID, dbusmenu_menuitem_get_id(m_gitem));

    GList *props = dbusmenu_menuitem_properties_list(m_gitem);
    GList *pointer = props;
    while(props) {
        QByteArray propName((const char*)props->data);
        if (mapedProperties.contains(propName) || propName.startsWith("x-")) {
            GVariant *var = dbusmenu_menuitem_property_get_variant(m_gitem, propName);
            updateProperty(propName, GVariantToQVariant(var));

            if (typeRelatedProperties.contains(propName)) {
                updateType();
            }
        }
        props = props->next;
    }
    g_list_free(pointer);
}

bool QDBusMenuItem::updateProperty(const QByteArray &name, QVariant value)
{
    qDebug() << "PROPERTY:" << name << "VALUE" << value;
    if (name == DBUSMENU_MENUITEM_PROP_LABEL) {
        setProperty(DBUSMENU_PROPERTY_LABEL, value);
    } else if (name == DBUSMENU_MENUITEM_PROP_ICON_NAME) {
        setProperty(DBUSMENU_PROPERTY_ICON_NAME, value);
    } else if (name == DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY) {
        setProperty(DBUSMENU_PROPERTY_HAS_SUBMENU, value.toString() == "submenu");
    } else if (name == DBUSMENU_MENUITEM_PROP_TOGGLE_STATE) {
        int newValue = -1;
        if (value.toInt() == DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED)
            newValue = 1;
        else if(value.toInt() == DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED)
            newValue = 0;
        setProperty(DBUSMENU_PROPERTY_STATE, newValue);
    } else if (name.startsWith("x-")) {
        m_extraProperties.insert(name.mid(2), value);
    } else {
        return false;
    }
    return true;
}

QDBusMenuItem *QDBusMenuItem::getChild(DbusmenuMenuitem *gitem) const
{
    Q_FOREACH(QObject *obj, children()) {
        QDBusMenuItem *i = qobject_cast<QDBusMenuItem*>(obj);
        if (i && i->item() == gitem)
            return i;
    }
    return 0;
}

QVariantMap QDBusMenuItem::extraProperties() const
{
    return m_extraProperties;
}
