
#include <QtWidgets>
#include "all-events-lane.h"
#include "controller-lane.h"
#include "inspector-sidebar.h"
#include "marker-lane.h"
#include "menu.h"
#include "midifile.h"
#include "note-lane.h"
#include "splitter.h"
#include "tempo-lane.h"
#include "time-ruler.h"
#include "velocity-lane.h"
#include "window.h"

QVector<Window*> Window::windows;

Window::Window(Window* existing_window)
{
	Window::windows.append(this);
	this->sequence = (existing_window == NULL) ? new Sequence() : existing_window->sequence;
	this->sequence->addWindow(this);
	this->statusBar();

	this->sidebar_splitter = new Splitter(Qt::Horizontal);
	this->setCentralWidget(sidebar_splitter);
	this->sidebar_splitter->setChildrenCollapsible(false);

	QWidget* lane_vbox = new QWidget();
	this->sidebar_splitter->addWidget(lane_vbox);
	QVBoxLayout* lane_vbox_layout = new QVBoxLayout(lane_vbox);
	lane_vbox_layout->setSpacing(0);
	lane_vbox_layout->setContentsMargins(0, 0, 0, 0);

	lane_vbox_layout->addWidget(new TimeRuler(this));

	this->lane_splitter = new Splitter(Qt::Vertical);
	lane_vbox_layout->addWidget(this->lane_splitter);
	this->lane_splitter->setChildrenCollapsible(false);

	this->lane_splitter->addWidget(new NoteLane(this));
	this->lane_splitter->addWidget(new VelocityLane(this));
	this->lane_splitter->addWidget(new ControllerLane(this));
	this->lane_splitter->addWidget(new TempoLane(this));
	this->lane_splitter->addWidget(new MarkerLane(this));
	this->lane_splitter->addWidget(new AllEventsLane(this));

	this->sidebar_tab_widget = new QTabWidget();
	this->sidebar_splitter->addWidget(this->sidebar_tab_widget);
	this->sidebar_tab_widget->setTabPosition(QTabWidget::North);
	this->sidebar_tab_widget->setDocumentMode(true);
	this->sidebar_tab_widget->setElideMode(Qt::ElideNone);
	this->sidebar_tab_widget->setUsesScrollButtons(false);

	this->inspector_sidebar = new InspectorSidebar(this);
	this->sidebar_tab_widget->addTab(inspector_sidebar, tr("Inspector"));

	this->sidebar_tab_widget->addTab(new QTextEdit("tracks placeholder"), tr("Tracks"));
	this->sidebar_tab_widget->addTab(new QTextEdit("channels placeholder"), tr("Channels"));

	QSettings settings;
	this->restoreGeometry(settings.value("window/window-geometry").toByteArray());
	this->restoreState(settings.value("window/window-state").toByteArray());
	this->lane_splitter->restoreState(settings.value("window/lane-splitter-state").toByteArray());
	this->sidebar_splitter->restoreState(settings.value("window/sidebar-splitter-state").toByteArray());
	this->sidebar_tab_widget->setCurrentIndex(settings.value("window/sidebar-tab-index", 0).toInt());
	this->use_linear_time = settings.value("window/use-linear-time", false).toBool();
	this->pixels_per_beat = settings.value("window/pixels-per-beat", 40.0).toFloat();
	this->pixels_per_second = settings.value("window/pixels-per-second", 40.0).toFloat();

	Menu::createMenuBar(this);
}

Window::~Window()
{
	this->sequence->removeWindow(this);
	Window::windows.removeOne(this);
}

void Window::closeEvent(QCloseEvent* event)
{
	if (Window::confirm_on_close && this->sequence->is_modified && (this->sequence->number_of_windows == 1) && !this->save(true, false, ""))
	{
		event->ignore();
		return;
	}

	QSettings settings;
	settings.setValue("window/window-geometry", this->saveGeometry());
	settings.setValue("window/window-state", this->saveState());
	settings.setValue("window/lane-splitter-state", this->lane_splitter->saveState());
	settings.setValue("window/sidebar-splitter-state", this->sidebar_splitter->saveState());
	settings.setValue("window/sidebar-tab-index", this->sidebar_tab_widget->currentIndex());
	settings.setValue("window/use-linear-time", this->use_linear_time);
}

void Window::newSequence()
{
	if (this->sequence->is_modified && (this->sequence->number_of_windows == 1) && !this->save(true, false, "")) return;
	this->sequence->removeWindow(this);
	this->sequence = new Sequence();
	this->sequence->addWindow(this);
	this->sequence->update(false);
}

void Window::newWindow()
{
	(new Window(this))->show();
}

void Window::open()
{
	if (this->sequence->is_modified && (this->sequence->number_of_windows == 1) && !this->save(true, false, "")) return;
	QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), QSettings().value("window/directory", "").toString(), tr("MIDI Files (*.mid)"));
	if (!filename.isEmpty()) this->open(filename);
}

void Window::open(QString filename)
{
	MidiFile_t new_midi_file = Sequence::loadMidiFile(filename);

	if (new_midi_file == NULL)
	{
		QMessageBox::warning(this, tr("Error"), tr("Cannot open the specified MIDI file."));
		return;
	}

	this->sequence->removeWindow(this);
	this->sequence = new Sequence();
	this->sequence->addWindow(this);
	this->sequence->filename = filename;
	MidiFile_free(this->sequence->midi_file);
	this->sequence->midi_file = new_midi_file;
	this->sequence->update(false);
	QSettings().setValue("window/directory", QFileInfo(filename).absolutePath());
}

void Window::save()
{
	this->save(false, false, "");
}

void Window::saveAs()
{
	this->save(false, true, "");
}

void Window::saveAs(QString filename)
{
	this->save(false, true, filename);
}

bool Window::save(bool ask_first, bool save_as, QString filename)
{
	if (ask_first)
	{
		switch (QMessageBox::question(this, tr("Save Changes"), tr("Do you want to save changes to the current file?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel))
		{
			case QMessageBox::Yes:
			{
				break;
			}
			case QMessageBox::No:
			{
				return true;
			}
			default:
			{
				return false;
			}
		}
	}

	if (this->sequence->filename.isEmpty() || save_as)
	{
		if (filename.isEmpty()) filename = QFileDialog::getSaveFileName(this, tr("Save File"), QSettings().value("window/directory", "").toString(), tr("MIDI Files (*.mid)"));

		if (filename.isEmpty())
		{
			return false;
		}
		else
		{
			if (this->sequence->saveAs(filename))
			{
				QSettings().setValue("window/directory", QFileInfo(filename).absolutePath());
				return true;
			}
		}
	}
	else
	{
		if (this->sequence->save())
		{
			return true;
		}
	}

	QMessageBox::warning(this, tr("Error"), tr("Cannot save the specified MIDI file."));
	return false;
}

void Window::quit()
{
	bool other_sequences_are_modified = false;

	for (Window* window: Window::windows)
	{
		if ((window->sequence != this->sequence) && (window->sequence->is_modified))
		{
			other_sequences_are_modified = true;
			break;
		}
	}

	if (other_sequences_are_modified)
	{
		if (QMessageBox::question(this, tr("Quit"), tr("Really quit without saving changes?"), QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok) return;
	}
	else
	{
		if (this->sequence->is_modified && !this->save(true, false, "")) return;
	}

	Window::confirm_on_close = false;
	QGuiApplication::quit();
}

void Window::undo()
{
	this->sequence->undo_stack->undo();
}

void Window::redo()
{
	this->sequence->undo_stack->redo();
}

void Window::delete_()
{
	for (MidiFileEvent_t midi_event = MidiFile_iterateEvents(this->sequence->midi_file); midi_event != NULL; midi_event = MidiFile_iterateEvents(this->sequence->midi_file))
	{
		if (MidiFileEvent_isSelected(midi_event)) MidiFileEvent_delete(midi_event);
	}

	this->sequence->update(true);
}

void Window::selectAll()
{
	Sequence::midiFileSelectAll(this->sequence->midi_file);
	this->sequence->update(false);
}

void Window::selectNone()
{
	Sequence::midiFileSelectNone(this->sequence->midi_file);
	this->sequence->update(false);
}

void Window::zoomInTime()
{
	this->zoomXBy(1.05);
}

void Window::zoomOutTime()
{
	this->zoomXBy(1 / 1.05);
}

void Window::addLane()
{
}

void Window::removeLane()
{
}

void Window::setUseLinearTime(bool use_linear_time)
{
	this->use_linear_time = use_linear_time;
	this->update();
}

void Window::aboutSeqer()
{
	QMessageBox::information(this, tr("About"), tr("Seqer\na MIDI sequencer by Div Slomin\nprovided under terms of the BSD license"));
}

void Window::underlyingSequenceUpdated()
{
	this->setWindowModified(this->sequence->is_modified);
	this->setWindowTitle(QString("%1[*] - Seqer").arg(this->sequence->filename.isEmpty() ? tr("Untitled") : this->sequence->filename));
	emit sequenceUpdated();
}

void Window::focusInspector()
{
	this->inspector_sidebar->setFocus(Qt::OtherFocusReason);
}

int Window::getXFromTick(long tick)
{
	if (this->use_linear_time)
	{
		return (int)(MidiFile_getTimeFromTick(this->sequence->midi_file, tick) * this->pixels_per_second - this->scroll_x);
	}
	else
	{
		return (int)(MidiFile_getBeatFromTick(this->sequence->midi_file, tick) * this->pixels_per_beat - this->scroll_x);
	}
}

long Window::getTickFromX(int x)
{
	if (this->use_linear_time)
	{
		return MidiFile_getTickFromTime(this->sequence->midi_file, (x + this->scroll_x) / this->pixels_per_second);
	}
	else
	{
		return MidiFile_getTickFromBeat(this->sequence->midi_file, (x + this->scroll_x) / this->pixels_per_beat);
	}
}

void Window::scrollXBy(float offset)
{
	float old_scroll_x = this->scroll_x;
	this->scroll_x = std::max(old_scroll_x - offset, 0.0f);
	this->cursor_x += (int)(old_scroll_x - this->scroll_x);
	this->update();
}

void Window::zoomXBy(float factor)
{
	long cursor_tick = this->getTickFromX(this->cursor_x);

	if (this->use_linear_time)
	{
		this->pixels_per_second *= factor;
	}
	else
	{
		this->pixels_per_beat *= factor;
	}

	this->cursor_x = this->getXFromTick(cursor_tick);
	this->update();
}

