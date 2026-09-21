import QtQuick

// Update capsule: a primary/subtle UiAlert with a single action.
// messageText/actionText/iconSource/actionTriggered come from UiAlert;
// messageText is kept as the historic alias for the title.
UiAlert {
    id: alertRoot

    property string messageText: ""

    title: alertRoot.messageText
    color: "primary"
    variant: "subtle"
    orientation: "horizontal"
}
