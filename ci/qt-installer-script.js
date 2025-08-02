// QT অটো ইনস্টলেশন স্ক্রিপ্ট (Linux/macOS)
function Controller() {
    installer.autoRejectMessageBoxes();
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", "Yes");
    
    // QT কম্পোনেন্ট সিলেকশন
    this.components = [
        "qt.qt6.5150.gcc_64",
        "qt.qt6.5150.qtwebengine",
        "qt.tools.cmake",
        "qt.tools.ninja"
    ];
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.selectComponents(this.components);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
    gui.clickButton(buttons.FinishButton);
}
