#include <QDateTime>
#include <QSet>
#include <QtDebug>

#include "../../utils.hpp"
#include "../chat/ChatModel.hpp"
#include "../core/CoreManager.hpp"

#include "SipAddressesModel.hpp"

// =============================================================================

SipAddressesModel::SipAddressesModel (QObject *parent) : QAbstractListModel(parent) {
  fetchSipAddresses();

  QObject::connect(
    CoreManager::getInstance()->getContactsListModel(), &ContactsListModel::contactAdded,
    this, &SipAddressesModel::updateFromContact
  );

  QObject::connect(
    CoreManager::getInstance()->getContactsListModel(), &ContactsListModel::contactRemoved,
    this, [this](const ContactModel *contact) {
      for (const auto &sip_address : contact->getVcardModel()->getSipAddresses()) {
        auto it = m_sip_addresses.find(sip_address.toString());
        if (it == m_sip_addresses.end()) {
          qWarning() << QStringLiteral("Unable to remove contact from sip address: `%1`.").arg(sip_address.toString());
          continue;
        }

        if (it->remove("contact") == 0)
          qWarning() << QStringLiteral("`contact` field is empty on sip address: `%1`.").arg(sip_address.toString());

        int row = m_refs.indexOf(&(*it));
        Q_ASSERT(row != -1);

        // History exists, signal changes.
        if (it->contains("timestamp")) {
          emit dataChanged(index(row, 0), index(row, 0));
          continue;
        }

        // Remove sip address if no history.
        removeRow(row);
      }
    }
  );
}

// -----------------------------------------------------------------------------

int SipAddressesModel::rowCount (const QModelIndex &) const {
  return m_refs.count();
}

QHash<int, QByteArray> SipAddressesModel::roleNames () const {
  QHash<int, QByteArray> roles;
  roles[Qt::DisplayRole] = "$sipAddress";
  return roles;
}

QVariant SipAddressesModel::data (const QModelIndex &index, int role) const {
  int row = index.row();

  if (!index.isValid() || row < 0 || row >= m_refs.count())
    return QVariant();

  if (role == Qt::DisplayRole)
    return QVariant::fromValue(*m_refs[row]);

  return QVariant();
}

// -----------------------------------------------------------------------------

ContactModel *SipAddressesModel::mapSipAddressToContact (const QString &sip_address) const {
  auto it = m_sip_addresses.find(sip_address);
  if (it == m_sip_addresses.end())
    return nullptr;

  return it->value("contact").value<ContactModel *>();
}

void SipAddressesModel::handleAllHistoryEntriesRemoved () {
  QObject *sender = QObject::sender();
  if (!sender)
    return;

  ChatModel *chat_model = qobject_cast<ChatModel *>(sender);
  if (!chat_model)
    return;

  const QString sip_address = chat_model->getSipAddress();
  auto it = m_sip_addresses.find(sip_address);
  if (it == m_sip_addresses.end()) {
    qWarning() << QStringLiteral("Unable to found sip address: `%1`.").arg(sip_address);
    return;
  }

  int row = m_refs.indexOf(&(*it));
  Q_ASSERT(row != -1);

  // No history, no contact => Remove sip address from list.
  if (!it->contains("contact")) {
    removeRow(row);
    return;
  }

  // Signal changes.
  it->remove("timestamp");
  emit dataChanged(index(row, 0), index(row, 0));
}

// -----------------------------------------------------------------------------

bool SipAddressesModel::removeRow (int row, const QModelIndex &parent) {
  return removeRows(row, 1, parent);
}

bool SipAddressesModel::removeRows (int row, int count, const QModelIndex &parent) {
  int limit = row + count - 1;

  if (row < 0 || count < 0 || limit >= m_sip_addresses.count())
    return false;

  beginRemoveRows(parent, row, limit);

  for (int i = 0; i < count; ++i) {
    const QVariantMap *map = m_refs.takeAt(row);
    QString sip_address = (*map)["sipAddress"].toString();
    qInfo() << QStringLiteral("Remove sip address: `%1`.").arg(sip_address);
    m_sip_addresses.remove(sip_address);
  }

  endRemoveRows();

  return true;
}

void SipAddressesModel::updateFromContact (ContactModel *contact) {
  for (const auto &sip_address : contact->getVcardModel()->getSipAddresses()) {
    const QString &sip_address_str = sip_address.toString();
    auto it = m_sip_addresses.find(sip_address_str);

    // New sip address from contact = new entry.
    if (it == m_sip_addresses.end()) {
      QVariantMap map;
      map["sipAddress"] = sip_address;
      map["contact"] = QVariant::fromValue(contact);

      m_sip_addresses[sip_address_str] = map;
      m_refs << &m_sip_addresses[sip_address_str];

      int row = m_refs.count() - 1;
      emit dataChanged(index(row, 0), index(row, 0));
      continue;
    }

    // Sip address exists, update contact.
    (*it)["contact"] = QVariant::fromValue(contact);

    int row = m_refs.indexOf(&(*it));
    Q_ASSERT(row != -1);
    emit dataChanged(index(row, 0), index(row, 0));
  }
}

void SipAddressesModel::fetchSipAddresses () {
  shared_ptr<linphone::Core> core = CoreManager::getInstance()->getCore();

  // Get sip addresses from chatrooms.
  for (const auto &chat_room : core->getChatRooms()) {
    list<shared_ptr<linphone::ChatMessage> > history = chat_room->getHistory(0);

    if (history.size() == 0)
      continue;

    QString sip_address = ::Utils::linphoneStringToQString(chat_room->getPeerAddress()->asString());

    QVariantMap map;
    map["sipAddress"] = sip_address;
    map["timestamp"] = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(history.back()->getTime()) * 1000);

    m_sip_addresses[sip_address] = map;
  }

  // Get sip addresses from calls.
  QSet<QString> address_done;
  for (const auto &call_log : core->getCallLogs()) {
    QString sip_address = ::Utils::linphoneStringToQString(call_log->getRemoteAddress()->asString());

    if (address_done.contains(sip_address))
      continue; // Already used.
    address_done << sip_address;

    QVariantMap map;
    map["sipAddress"] = sip_address;
    map["timestamp"] = QDateTime::fromMSecsSinceEpoch(
        static_cast<qint64>(call_log->getStartDate() + call_log->getDuration()) * 1000
      );

    auto it = m_sip_addresses.find(sip_address);
    if (it == m_sip_addresses.end() || map["timestamp"] > (*it)["timestamp"])
      m_sip_addresses[sip_address] = map;
  }

  for (const auto &map : m_sip_addresses)
    m_refs << &map;

  // Get sip addresses from contacts.
  for (auto &contact : CoreManager::getInstance()->getContactsListModel()->m_list)
    updateFromContact(contact);
}
