/*
 * ScreenRecorderControlWidget.h - recording control widget for Veyon Master
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

namespace Ui {
class ScreenRecorderControlWidget;
}

class ScreenRecorderControlWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ScreenRecorderControlWidget( QWidget* parent = nullptr );
	~ScreenRecorderControlWidget() override;

	void start( int computerCount, const QString& outputFolder );
	void stop( const QString& finalDuration = {} );
	void setComputerCount( int count );
	void setOutputFolder( const QString& folder );

	bool isRecording() const;
	QString currentDurationFormatted() const;

Q_SIGNALS:
	void stopRecordingRequested();

protected:
	void closeEvent( QCloseEvent* event ) override;

private Q_SLOTS:
	void onTimerTick();
	void onStopButtonClicked();
	void onOpenFolderButtonClicked();
	void onCloseButtonClicked();

private:
	Ui::ScreenRecorderControlWidget* ui;
	QTimer m_tickTimer;
	QElapsedTimer m_elapsedTimer;
	QString m_outputFolder;
	bool m_recording{false};
	bool m_indicatorBlink{false};
	int m_computerCount{0};
};
