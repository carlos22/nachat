#include <QtNetwork>
#include <QApplication>
#include <QMessageBox>
#include <QSettings>

#include "matrix/Matrix.hpp"
#include "matrix/Session.hpp"

#include "LoginDialog.hpp"
#include "MainWindow.hpp"

#include "version.hpp"

int main(int argc, char *argv[]) {
  printf("NaChat %s\n", version::string().toStdString().c_str());

  QCoreApplication::setOrganizationName("nachat");
  QCoreApplication::setApplicationName("nachat");
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QApplication app(argc, argv);
  QSettings settings;

  QNetworkAccessManager net;   // Create this on another thread for faster startup?
  matrix::Matrix matrix{net};

  LoginDialog login;
  std::unique_ptr<MainWindow> main_window;
  std::unique_ptr<matrix::Session> session;

  auto &&session_established = [&]() {
    QObject::connect(session.get(), &matrix::Session::logged_out, [&]() {
        settings.remove("session/access_token");
        settings.remove("session/user_id");

        main_window.reset();

        // Pass ownership to Qt for disposal
        session->deleteLater();
        session.release();

        login.show();
      });
    main_window = std::make_unique<MainWindow>(*session);
    QObject::connect(main_window.get(), &MainWindow::quit, &app, &QApplication::quit);
    main_window->show();
  };

  QObject::connect(&matrix, &matrix::Matrix::logged_in, [&](const QString &user_id, const QString &access_token) {
      session = matrix::Session::create(matrix, login.homeserver(), user_id, access_token);
      settings.setValue("login/username", login.username());
      settings.setValue("login/homeserver", login.homeserver());
      settings.setValue("session/access_token", access_token);
      settings.setValue("session/user_id", user_id);
      login.hide();
      login.setDisabled(false);
      session_established();
    });

  QObject::connect(&matrix, &matrix::Matrix::login_error, [&](QString err){
      login.setDisabled(false);
      auto msg = new QMessageBox(QMessageBox::Critical, QObject::tr("Login Error"), err, QMessageBox::Ok, &login);
      msg->setAttribute(Qt::WA_DeleteOnClose);
      msg->open();
    });

  QObject::connect(&login, &LoginDialog::accepted, [&](){
      matrix.login(login.homeserver(), login.username(), login.password());
    });

  auto homeserver = settings.value("login/homeserver");
  auto access_token = settings.value("session/access_token");
  auto user_id = settings.value("session/user_id");
  if(access_token.isNull() || homeserver.isNull() || user_id.isNull()) {
    login.show();
  } else {
    session = matrix::Session::create(matrix, homeserver.toString(), user_id.toString(), access_token.toString());
    session_established();
  }

  return app.exec();
}
