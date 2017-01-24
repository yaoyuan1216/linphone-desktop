import QtQuick 2.7
import QtQuick.Layouts 1.3

import Common 1.0
import Linphone 1.0
import Linphone.Styles 1.0

// =============================================================================

Rectangle {
  id: callControls

  // ---------------------------------------------------------------------------

  default property alias _content: content.data

  property alias signIcon: signIcon.icon
  property alias sipAddressColor: contact.sipAddressColor
  property alias usernameColor: contact.usernameColor
  property string sipAddress

  // ---------------------------------------------------------------------------

  property var _contactObserver: SipAddressesModel.getContactObserver(sipAddress)

  // ---------------------------------------------------------------------------

  signal clicked

  // ---------------------------------------------------------------------------

  color: CallControlsStyle.color
  height: CallControlsStyle.height
  width: CallControlsStyle.width

  MouseArea {
    anchors.fill: parent

    onClicked: callControls.clicked()
  }

  RowLayout {
    anchors {
      fill: parent
      leftMargin: CallControlsStyle.leftMargin
      rightMargin: CallControlsStyle.rightMargin
    }

    spacing: 0

    Contact {
      id: contact

      Layout.fillHeight: true
      Layout.fillWidth: true

      displayUnreadMessagesCount: true

      entry: ({
        contact: _contactObserver.contact,
        sipAddress: callControls.sipAddress
      })
    }

    Item {
      id: content

      Layout.fillHeight: true

      Component.onCompleted: Layout.preferredWidth = data[0].width
    }
  }

  // ---------------------------------------------------------------------------

  Icon {
    id: signIcon

    anchors {
      left: parent.left
      top: parent.top
    }

    iconSize: CallControlsStyle.signSize
  }
}
