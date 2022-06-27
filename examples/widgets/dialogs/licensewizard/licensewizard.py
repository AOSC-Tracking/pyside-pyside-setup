#############################################################################
##
## Copyright (C) 2022 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt for Python examples of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:BSD$
## You may use this file under the terms of the BSD license as follows:
##
## "Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##   * Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##   * Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in
##     the documentation and/or other materials provided with the
##     distribution.
##   * Neither the name of The Qt Company Ltd nor the names of its
##     contributors may be used to endorse or promote products derived
##     from this software without specific prior written permission.
##
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
##
## $QT_END_LICENSE$
##
#############################################################################

import sys
from enum import IntEnum
from pathlib import Path

from PySide6.QtCore import QRegularExpression
from PySide6.QtGui import QPixmap, QRegularExpressionValidator
from PySide6.QtPrintSupport import QPrintDialog, QPrinter
from PySide6.QtWidgets import (QCheckBox, QGridLayout, QLabel, QLineEdit,
                               QMessageBox, QRadioButton, QVBoxLayout, QWizard,
                               QWizardPage)

EMAIL_REGEXP = ".+@.+"


class Pages(IntEnum):
    Page_Intro = 0
    Page_Evaluate = 1
    Page_Register = 2
    Page_Details = 3
    Page_Conclusion = 4


class IntroPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.setTitle("Introduction")

        path = Path(__file__).resolve().parent

        self.setPixmap(QWizard.WatermarkPixmap, QPixmap(path / "images" / "watermark.png"))
        self.top_label = QLabel(
            "This wizard will help you register your copy of "
            "<i>Super Product One</i>&trade; or start "
            "evaluating the product"
        )
        self.top_label.setWordWrap(True)

        self.register_radio_button = QRadioButton("&Register your copy")
        self.register_radio_button.setChecked(True)

        self.evaluate_radio_button = QRadioButton("&Evaluate the product for 30 days")
        layout = QVBoxLayout(self)
        layout.addWidget(self.top_label)
        layout.addWidget(self.register_radio_button)
        layout.addWidget(self.evaluate_radio_button)

    def nextId(self):
        if self.evaluate_radio_button.isChecked():
            return Pages.Page_Evaluate
        else:
            return Pages.Page_Register


class EvaluatePage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("Evaluate <i>Super Product One</i>&trade")
        self.setSubTitle(
            "Please fill both fields. Make sure to provide a valid "
            "email address (e.g., john.smith@example.com)."
        )

        self.name_label = QLabel("N&ame:")
        self.name_line_edit = QLineEdit()

        self.name_label.setBuddy(self.name_line_edit)

        self.email_label = QLabel("&Email address:")
        self.email_line_edit = QLineEdit()
        self.email_line_edit.setValidator(
            QRegularExpressionValidator(QRegularExpression(EMAIL_REGEXP), self)
        )
        self.email_label.setBuddy(self.email_line_edit)

        self.registerField("evaluate.name*", self.name_line_edit)
        self.registerField("evaluate.email*", self.email_line_edit)

        layout = QGridLayout(self)
        layout.addWidget(self.name_label, 0, 0)
        layout.addWidget(self.name_line_edit, 0, 1)
        layout.addWidget(self.email_label, 1, 0)
        layout.addWidget(self.email_line_edit, 1, 1)

    def nextId(self):
        return Pages.Page_Conclusion


class RegisterPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("Register Your Copy of <i>Super Product One</i>&trade")
        self.setSubTitle("If you have an upgrade key, please fill in " "the appropriate field.")

        self.name_label = QLabel("N&ame:")
        self.name_line_edit = QLineEdit()
        self.name_label.setBuddy(self.name_line_edit)

        self.upgrade_key_label = QLabel("&Upgrade key:")
        self.upgrade_key_line_edit = QLineEdit()
        self.upgrade_key_label.setBuddy(self.upgrade_key_line_edit)

        self.registerField("register.name*", self.name_line_edit)
        self.registerField("register.upgradeKey", self.upgrade_key_line_edit)

        layout = QGridLayout(self)
        layout.addWidget(self.name_label, 0, 0)
        layout.addWidget(self.name_line_edit, 0, 1)
        layout.addWidget(self.upgrade_key_label, 1, 0)
        layout.addWidget(self.upgrade_key_line_edit, 1, 1)

    def nextId(self):
        if self.upgrade_key_line_edit.text():
            return Pages.Page_Details
        else:
            return Pages.Page_Conclusion


class DetailsPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("Fill In Your Details")
        self.setSubTitle(
            "Please fill all three fields. Make sure to provide a valid "
            "email address (e.g., tanaka.aya@example.co.jp)."
        )

        self.company_label = QLabel("&Company name:")
        self.company_line_edit = QLineEdit()
        self.company_label.setBuddy(self.company_line_edit)

        self.email_label = QLabel("&Email address:")
        self.email_line_edit = QLineEdit()
        self.email_line_edit.setValidator(
            QRegularExpressionValidator(QRegularExpression(EMAIL_REGEXP), self)
        )
        self.email_label.setBuddy(self.email_line_edit)

        self.postal_label = QLabel("&Postal address:")
        self.postal_line_edit = QLineEdit()
        self.postal_label.setBuddy(self.postal_line_edit)

        self.registerField("details.company*", self.company_line_edit)
        self.registerField("details.email*", self.email_line_edit)
        self.registerField("details.postal*", self.postal_line_edit)

        layout = QGridLayout(self)
        layout.addWidget(self.company_label, 0, 0)
        layout.addWidget(self.company_line_edit, 0, 1)
        layout.addWidget(self.email_label, 1, 0)
        layout.addWidget(self.email_line_edit, 1, 1)
        layout.addWidget(self.postal_label, 2, 0)
        layout.addWidget(self.postal_line_edit, 2, 1)

    def nextId(self):
        return Pages.Page_Conclusion


class ConclusionPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("Complete Your Registration")

        path = Path(__file__).resolve().parent
        self.setPixmap(QWizard.WatermarkPixmap, QPixmap(path / "images" / "watermark.png"))

        self.bottom_label = QLabel()
        self.bottom_label.setWordWrap(True)

        agreeCheckBox = QCheckBox("I agree to the terms of the license")

        self.registerField("conclusion.agree*", agreeCheckBox)

        layout = QVBoxLayout(self)
        layout.addWidget(self.bottom_label)
        layout.addWidget(agreeCheckBox)

        self.custom_button_clicked_signal_connected = False

    def nextId(self):
        return -1

    def initializePage(self):
        if self.wizard().hasVisitedPage(Pages.Page_Evaluate):
            license_text = "<u>Evaluation License Agreement:</u> "
            "You can use self software for 30 days and make one "
            "backup, but you are not allowed to distribute it."
        elif self.wizard().hasVisitedPage(Pages.Page_Details):
            license_text = (
                "<u>First-Time License Agreement:</u> "
                "You can use self software subject to the license "
                "you will receive by email."
            )

        else:
            license_text = (
                "<u>Upgrade License Agreement:</u> "
                "This software is licensed under the terms of your "
                "current license."
            )
        self.bottom_label.setText(license_text)

    def setVisible(self, visible: bool):
        super().setVisible(visible)
        if visible:
            self.wizard().setButtonText(QWizard.CustomButton1, "&Print")
            self.wizard().setOption(QWizard.HaveCustomButton1, True)

            if not self.custom_button_clicked_signal_connected:
                self.custom_button_clicked_signal_connected = True
                self.wizard().customButtonClicked.connect(self.print_button_clicked)
        else:
            self.wizard().setOption(QWizard.HaveCustomButton1, False)

            if self.custom_button_clicked_signal_connected:
                self.custom_button_clicked_signal_connected = False
                self.wizard().customButtonClicked.disconnect(self.print_button_clicked)

    def print_button_clicked(self):
        printer = QPrinter()

        dialog = QPrintDialog(printer, self)
        if dialog.exec():
            QMessageBox.warning(
                self,
                "Print License",
                "As an environmentally friendly measure, the "
                "license text will not actually be printed.",
            )


class LicenseWizard(QWizard):
    def __init__(self, parent=None):
        super().__init__(parent)

        self._pages = [
            IntroPage(),
            EvaluatePage(),
            RegisterPage(),
            DetailsPage(),
            ConclusionPage()
        ]

        for page in self._pages:
            self.addPage(page)

        self.setStartId(Pages.Page_Intro)

        if sys.platform == 'darwin':
            self.setWizardStyle(QWizard.ModernStyle)

        self.setOption(QWizard.HaveHelpButton, True)

        path = Path(__file__).resolve().parent
        self.setPixmap(QWizard.LogoPixmap, QPixmap(path / "images" / "logo.png"))

        self.helpRequested.connect(self.show_help)
        self.setWindowTitle("License Wizard")

        self.last_help_message: str = None

    def show_help(self):
        if self.currentId() == Pages.Page_Intro:
            message = "The decision you make here will affect which page you get to see next."

        elif self.currentId() == Pages.Page_Evaluate:
            message = (
                "Make sure to provide a valid email address, such as "
                "toni.buddenbrook@example.de."
            )

        elif self.currentId() == Pages.Page_Register:
            message = (
                "If you don't provide an upgrade key, you will be asked to fill in your details."
            )

        elif self.currentId() == Pages.Page_Details:
            message = (
                "Make sure to provide a valid email address, such as "
                "thomas.gradgrind@example.co.uk."
            )

        elif self.currentId() == Pages.Page_Conclusion:
            message = "You must accept the terms and conditions of the license to proceed."
        else:
            message = "This help is likely not to be of any help."

        if self.last_help_message == message:
            message = (
                "Sorry, I already gave what help I could. Maybe you should try asking a human?"
            )

        QMessageBox.information(self, "License Wizard Help", message)

        self.last_help_message = message