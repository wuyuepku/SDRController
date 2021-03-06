#include "SwitchesShow.h"

// you can set those data
static double gap3line = 0.08;
static double gapcenterline = 0.25;
static double normCircleSize = 0.1;
static double normToPixel = 100;
static double biasX = 100;
static double biasY = 100;

#ifdef QT_DEBUG
static int SDRprintf(const char* format, ...)
{
	char buf[1024];
	int i;
	va_list vlist;
	va_start(vlist, format);
	i = vsprintf(buf, format, vlist);
	va_end(vlist);
	qDebug() << buf;
	return i;
}
#else
int SDRprintf(const char* format, ...) {}
#endif
#define SDRerror(format, ...) SDRprintf("error %s(%d)" format , __FILE__, __LINE__, ##__VA_ARGS__)

int SwitchesShow::importConfig(string jsonstr)
{
	QJsonParseError complex_json_error;
	QJsonDocument complex_parse_doucment = QJsonDocument::fromJson(QString::fromStdString(jsonstr).toLatin1(), &complex_json_error);
	if (complex_json_error.error == QJsonParseError::NoError) {
		if (complex_parse_doucment.isObject()) {
			QJsonObject json = complex_parse_doucment.object();
			if (json.contains("version")) {
				QJsonValue val = json.take("version");
				if (val.isString()) {
					QString version = val.toString();
					qDebug() << "configure file version is " << version;
				}
			}
			if (json.contains("GPIOExts")) {
				mutex.lock();
				gpioexts.clear();
				extraInit();
				QJsonValue val = json.take("GPIOExts");
				if (val.isArray()) {
					QJsonArray arr = val.toArray();
					for (QJsonArray::iterator it = arr.begin(); it != arr.end(); ++it) {
						QJsonValue v = *it;
						GPIOExt tmp;
						if (v.isObject()) {
							QJsonObject dev = v.toObject();
							if (dev.contains("id")) {
								QJsonValue devv = dev.take("id");
								if (devv.isDouble()) tmp.id = devv.toInt();
							}
							if (dev.contains("bind")) {
								QJsonValue devv = dev.take("bind");
								if (devv.isArray()) {
									QJsonArray bind = devv.toArray();
									if (bind.size() > 0 && bind[0].isString()) {
										tmp.bindType = bind[0].toString();
									}
									if (bind.size() > 1 && bind[1].isDouble()) {
										tmp.bindId = bind[1].toInt();
									}
									if (bind.size() > 2 && bind[2].isString()) {
										tmp.bindMethod = bind[2].toString();
									}
								}
							}
						}
						gpioexts.insert(tmp);
					}
				}
				mutex.unlock();
			}
			if (json.contains("switches")) {
				mutex.lock();
				lines.clear(); // clear the set
				extraInit();
				QJsonValue val = json.take("switches");
				if (val.isArray()) {
					QJsonArray arr = val.toArray();
					for (QJsonArray::iterator it = arr.begin(); it != arr.end(); ++it) {
						QJsonValue v = *it;
						if (v.isString()) {
							QString node = v.toString();
							QStringList list = node.split(' ');
							if (list.size() != 6) {
								SDRerror("QStringList size is not 4");
								continue;
							}
							Switch3Line tmp;
							tmp.a.alpha = list[0].toInt();
							tmp.a.beta = list[1].toInt();
							tmp.b.alpha = list[2].toInt();
							tmp.b.beta = list[3].toInt();
							tmp.bindExtId = list[4].toInt();
							tmp.bindExtGPIO = list[5]; // GPIO name
							lines.insert(tmp);
						}
					}
				}
				mutex.unlock();
				update(); // need to explicitly call the repaint function
			}
			else {
				SDRerror("json does not contain \"switches\", remain now state");
			}
			if (json.contains("ESP32")) {
				mutex.lock();
				esp32s.clear();
				extraInit();
				QJsonValue val = json.take("ESP32");
				if (val.isArray()) {
					QJsonArray arr = val.toArray();
					for (QJsonArray::iterator it = arr.begin(); it != arr.end(); ++it) {
						QJsonValue v = *it;
						ESP32 esp32;
						if (v.isObject()) {
							QJsonObject dev = v.toObject();
							if (dev.contains("id")) {
								QJsonValue devv = dev.take("id");
								if (devv.isDouble()) {
									esp32.id = devv.toInt();
								}
							}
							if (dev.contains("name")) {
								QJsonValue devv = dev.take("name");
								if (devv.isString()) {
									esp32.name = devv.toString();
								}
							}
							if (dev.contains("addr")) {
								QJsonValue devv = dev.take("addr");
								if (devv.isString()) {
									esp32.addr = devv.toString();
								}
							}
							if (dev.contains("port")) {
								QJsonValue devv = dev.take("port");
								if (devv.isDouble()) {
									esp32.port = devv.toInt();
								}
							}
							if (dev.contains("bootscript")) {
								QJsonValue devv = dev.take("bootscript");
								if (devv.isString()) {
									esp32.bootscript = devv.toString();
								}
							}
						}
						esp32s.insert(esp32);
					}
				}
				// TODO: connect all
				for (auto it = esp32s.begin(); it != esp32s.end(); ++it) {
					it->tryConnect();
					if (it->socket.state() == QAbstractSocket::ConnectedState) {
						it->probeAndSync(*this);
					}
				}
				update(); // need to explicitly call the repaint function
				mutex.unlock();
			}
			if (json.contains("NFCWPTmod")) {

			}
			if (json.contains("plugins")) {
				mutex.lock();
				plugins.clear();
				extraInit();
				QJsonValue val = json.take("plugins");
				if (val.isArray()) {
					QJsonArray arr = val.toArray();
					for (QJsonArray::iterator it = arr.begin(); it != arr.end(); ++it) {
						QJsonValue v = *it;
						if (v.isObject()) {
							QJsonObject dev = v.toObject();
							PluginDevice tmp;
							if (dev.contains("name")) {
								QJsonValue devv = dev.take("name");
								if (devv.isString()) tmp.name = devv.toString();
							}
							if (dev.contains("position")) {
								QJsonValue devv = dev.take("position");
								if (devv.isString()) {
									QStringList list = devv.toString().split(' ');
									if (list.size() != 2) {
										SDRerror("QStringList size is not 2");
										continue;
									}
									tmp.position.alpha = list[0].toInt();
									tmp.position.beta = list[1].toInt();
								}
							}
							if (dev.contains("descriptor")) {
								QJsonValue devv = dev.take("descriptor");
								if (devv.isString()) tmp.descriptor = devv.toString();
							}
							if (dev.contains("bind")) {
								QJsonValue devv = dev.take("bind");
								if (devv.isArray()) {
									QJsonArray bind = devv.toArray();
									if (bind.size() > 0 && bind[0].isString()) {
										tmp.bindType = bind[0].toString();
									}
									if (bind.size() > 1 && bind[1].isDouble()) {
										tmp.bindId = bind[1].toInt();
									}
									if (bind.size() > 2 && bind[2].isString()) {
										tmp.bindMethod = bind[2].toString();
									}
								}
							}
							plugins.insert(tmp);
						}
					}
				}
				mutex.unlock();
				update(); // need to explicitly call the repaint function
			}
			return 0;
		}
		return -2;
	}
	return -1;
}

string SwitchesShow::exportConfig()
{
	QJsonObject json;
	json["update"] = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss");
	json["version"] = SDRVersion::version();
	mutex.lock();
	QJsonArray switches;
	for (set<Switch3Line>::iterator it = lines.begin(); it != lines.end(); ++it) {
		QString str;
		str.sprintf("%d %d %d %d %d %s", it->a.alpha, it->a.beta, it->b.alpha, it->b.beta, it->bindExtId, it->bindExtGPIO.toLatin1().data());
		switches.append(str);
	}
	QJsonArray plgins;
	for (set<PluginDevice>::iterator it = plugins.begin(); it != plugins.end(); ++it) {
		QString str;
		str.sprintf("%d %d", it->position.alpha, it->position.beta);
		QJsonObject dev;
		dev["name"] = it->name;
		dev["position"] = str;
		dev["descriptor"] = it->descriptor;
		QJsonArray bind;
		bind.append(it->bindType);
		bind.append(it->bindId);
		bind.append(it->bindMethod);
		dev["bind"] = bind;
		plgins.append(dev);
	}
	QJsonArray esps;
	for (set<ESP32>::iterator it = esp32s.begin(); it != esp32s.end(); ++it) {
		QJsonObject dev;
		dev["id"] = it->id;
		dev["name"] = it->name;
		dev["addr"] = it->addr.toString();
		dev["port"] = it->port;
		dev["bootscript"] = it->bootscript;
		esps.append(dev);
	}
	QJsonArray gpios;
	for (set<GPIOExt>::iterator it = gpioexts.begin(); it != gpioexts.end(); ++it) {
		QJsonObject dev;
		dev["id"] = it->id;
		QJsonArray bind;
		bind.append(it->bindType);
		bind.append(it->bindId);
		bind.append(it->bindMethod);
		dev["bind"] = bind;
		gpios.append(dev);
	}
	mutex.unlock();
	json["switches"] = switches;
	json["plugins"] = plgins;
	json["ESP32"] = esps;
	json["GPIOExts"] = gpios;
	QJsonDocument document(json);
	return QString(document.toJson()).toStdString();
}

void SwitchesShow::lockDataMutex()
{
	mutex.lock();
}

void SwitchesShow::unlockDataMutex()
{
	mutex.unlock();
	update(); // this way, after changing the data, the picture will change
}

void SwitchesShow::enabletreeWidget_ESP32s(QTreeView * treeWidget_ESP32s_)
{
	treeView_ESP32s = treeWidget_ESP32s_;
}

void SwitchesShow::paintEvent(QPaintEvent * event)
{
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	mutex.lock();
	autoFitToShow(); // auto fit
					 //painter.setBrush
	set<IntPoint> & nodes = paintedNodes;
	nodes.clear();
	for (set<Switch3Line>::iterator it = lines.begin(); it != lines.end(); ++it) {
		autoRotateSwitch3Lines(*it); // auto rotate the switches
		vector<double> pl = it->giveme3line();
		nodes.insert(it->a);
		nodes.insert(it->b);
		for (size_t j = 0; j + 3 < pl.size(); j += 4) {
			painter.setPen(QPen(it->getColor(j / 4), 3));
			painter.drawLine(toPixelX(pl[j]), toPixelY(pl[j + 1])
				, toPixelX(pl[j + 2]), toPixelY(pl[j + 3]));
		}
		// draw polygan
		painter.setPen(QPen(QColor("lightGrey"), 1));
		vector<double> plg = getPolygons(it->a, it->b);
		QPointF* plgps = new QPointF[plg.size() / 2];
		for (size_t j = 0; j + 1 < plg.size(); j += 2) {
			plgps[j / 2].setX(plg[j]);
			plgps[j / 2].setY(plg[j + 1]);
		}
		painter.drawPolygon(plgps, plg.size() / 2);
		delete[] plgps;
	}

	for (set<PluginDevice>::iterator it = plugins.begin(); it != plugins.end(); ++it) {
		nodes.insert(it->position);
	}
	PluginDevice tmpdev;
	for (set<IntPoint>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		painter.setPen(QPen(QColor(0, 160, 230), 2));
		painter.setBrush(QColor(0, 0, 0, 0));
		tmpdev.position = *it;
		set<PluginDevice>::iterator fnd = plugins.find(tmpdev);
		if (fnd != plugins.end()) {
			painter.setBrush(fnd->getColor());
		}
		painter.drawEllipse(QPointF(toPixelX(it->x()), toPixelY(it->y())), normToPixel * normCircleSize, normToPixel * normCircleSize);
	} painter.setBrush(QColor(0, 0, 0, 0));

	if (isfocusIntPoint) {
		painter.setPen(QPen(QColor(255, 0, 0), 3));
		painter.setBrush(QColor(0, 0, 0, 0));
		painter.drawEllipse(QPointF(toPixelX(focusedIntPoint.x()), toPixelY(focusedIntPoint.y()))
			, normToPixel * normCircleSize * 1.5, normToPixel * normCircleSize * 1.5);
	}

	updatetreeWidget_ESP32s();

	mutex.unlock();
}

void SwitchesShow::mouseDoubleClickEvent(QMouseEvent * e)
{
	SDRprintf("mouseDoubleClickEvent");
	QPoint p = e->pos();
	double x = p.x(), y = p.y();
	// not consider efficiency, because it's a debuggin tool
	QString str;
	mutex.lock();
	double R2 = normCircleSize * normToPixel;
	R2 *= R2;
	isfocusIntPoint = false;
	for (set<IntPoint>::iterator it = paintedNodes.begin(); it != paintedNodes.end(); ++it) {
		double delx = toPixelX(it->x()) - x;
		double dely = toPixelY(it->y()) - y;
		if (delx*delx + dely * dely < R2) { // found
			isfocusIntPoint = true;
			focusedIntPoint = *it;
			str.sprintf("axis is (%d, %d)", it->alpha, it->beta);
			PluginDevice tmpdev;
			tmpdev.position = *it;
			set<PluginDevice>::iterator fnd = plugins.find(tmpdev);
			if (fnd != plugins.end()) {
				str += QString() + "\n(\"" + fnd->name + "\", \"" + fnd->descriptor + "\")";
			}
		}
	}
	update();
	mutex.unlock();
	if (str.length() > 0) QMessageBox::about(NULL, "Node", str);
}

void SwitchesShow::keyPressEvent(QKeyEvent * event)
{
	qDebug() << "keyPressEvent";
	if (isfocusIntPoint) {
		keypressed = event->key();
		keypressedTime = QTime::currentTime();
	}
}

void SwitchesShow::keyReleaseEvent(QKeyEvent * event)
{
	qDebug() << "keyReleaseEvent";
	if (keypressed == -1) return;
	if (event->key() == keypressed) {
		int dx = -2, dy = -2;
		if (keypressed == Qt::Key_W) {
			dx = 0; dy = -1;
		}
		else if (keypressed == Qt::Key_E) {
			dx = -1; dy = 0;
		}
		else if (keypressed == Qt::Key_R) {
			dx = -1; dy = 1;
		}
		else if (keypressed == Qt::Key_Z) {
			dx = 1; dy = -1;
		}
		else if (keypressed == Qt::Key_X) {
			dx = 1; dy = 0;
		}
		else if (keypressed == Qt::Key_C) {
			dx = 0; dy = 1;
		}
		if (dx != -2 && dy != -2) {
			QTime tar = QTime::currentTime().addMSecs(-300);
			Switch3Line tmp;
			tmp.a = focusedIntPoint;
			tmp.b.alpha = tmp.a.alpha + dx;
			tmp.b.beta = tmp.a.beta + dy;
			mutex.lock();
			if (tar < keypressedTime) { // add new line
				lines.insert(tmp);
			}
			else { // delete
				lines.erase(tmp);
			}
			mutex.unlock();
		}
	}
	keypressed = -1;
	update();
}

vector<double> SwitchesShow::getPolygons(const IntPoint & a, const IntPoint & b)
{
	vector<double> v = { 0.2, 0.15, 0.35, 0.25, 0.65, 0.25, 0.8, 0.15, 0.8, -0.15, 0.2, -0.15 };
	double x1 = toPixelX(a.x()), y1 = toPixelY(a.y());
	double x2 = toPixelX(b.x()), y2 = toPixelY(b.y());
	double delX = x2 - x1, delY = y2 - y1;
	for (size_t i = 0; i + 1 < v.size(); i += 2) {
		double x = v[i];
		double y = v[i + 1];
		double ax = x1 + delX * x;
		double ay = y1 + delY * x;
		ax -= delY * y;
		ay += delX * y;
		v[i] = ax;
		v[i + 1] = ay;
	}
	return v;
}

void SwitchesShow::autoRotateSwitch3Lines(const Switch3Line & l)
{
	IntPoint& a = l.a;
	IntPoint& b = l.b;
	if (a.beta == b.beta) {
		if (b.alpha == a.alpha + 1) return;
		if (a.alpha == b.alpha + 1) {
			IntPoint c = a; a = b; b = c; return;
		}
	}
	if (a.alpha == b.alpha) {
		if (b.beta == a.beta + 1) return;
		if (a.beta == b.beta + 1) {
			IntPoint c = a; a = b; b = c; return;
		}
	}
	if (a.alpha == b.alpha - 1 && a.beta == b.beta + 1) return;
	if (b.alpha == a.alpha - 1 && b.beta == a.beta + 1) {
		IntPoint c = a; a = b; b = c; return;
	}
	SDRerror("autoRotateSwitch3Lines failed: a.alpha=%d, a.beta=%d, b.alpha=%d, b.beta=%d"
		, a.alpha, a.beta, b.alpha, b.beta);
}

double SwitchesShow::toPixelX(double normalx)
{
	return biasX + normToPixel * normalx;
}

double SwitchesShow::toPixelY(double normaly)
{
	return biasY + normToPixel * normaly;
}

void SwitchesShow::autoFitToShow()
{
	if (!isAutoFitToShow) return;
	if (lines.empty()) return;
	double minx, maxx, miny, maxy;
	set<Switch3Line>::iterator it = lines.begin();
	double x1 = it->a.x(), y1 = it->a.y();
	double x2 = it->b.x(), y2 = it->b.y();
	if (x1 < x2) {
		minx = x1; maxx = x2;
	}
	else { minx = x2; maxx = x1; }
	if (y1 < y2) {
		miny = y1; maxy = y2;
	}
	else { miny = y2; maxy = y1; }
	for (; it != lines.end(); ++it) {
		x1 = it->a.x(), y1 = it->a.y();
		x2 = it->b.x(), y2 = it->b.y();
		if (x1 < minx) minx = x1;
		if (x2 < minx) minx = x2;
		if (x1 > maxx) maxx = x1;
		if (x2 > maxx) maxx = x2;
		if (y1 < miny) miny = y1;
		if (y2 < miny) miny = y2;
		if (y1 > maxy) maxy = y1;
		if (y2 > maxy) maxy = y2;
	}
	//SDRprintf("x:(%lf, %lf), y:(%lf, %lf)", minx, maxx, miny, maxy);
	double w = width(), h = height();
	//SDRprintf("w=%lf, h=%lf", w, h);
	double nTPx = w / (maxx - minx) * 0.8;
	double nTPy = w / (maxy - miny) * 0.8;
	normToPixel = nTPx < nTPy ? nTPx : nTPy;
	biasX = w / 2 - (minx + maxx) / 2 * normToPixel;
	biasY = h / 2 - (miny + maxy) / 2 * normToPixel;
}

void SwitchesShow::extraInit()
{
	isfocusIntPoint = false;
	keypressed = -1;
}

void SwitchesShow::updatetreeWidget_ESP32s()
{
	if (treeView_ESP32s == NULL) return;
	QStandardItemModel* model = new QStandardItemModel(treeView_ESP32s);
	model->setHorizontalHeaderLabels(QStringList() << QStringLiteral("key") << QStringLiteral("value"));
	for (auto it = esp32s.begin(); it != esp32s.end(); ++it) {
		QStandardItem* itemESP32 = new QStandardItem(QString("ESP32")); model->appendRow(itemESP32);
		model->setItem(itemESP32->index().row(), 1, new QStandardItem(QString().sprintf("[%d]", it->id)));
		QStandardItem* nameESP32 = new QStandardItem(QString("name")); itemESP32->appendRow(nameESP32);
		itemESP32->setChild(nameESP32->index().row(), 1, new QStandardItem(it->name));
		QStandardItem* tarESP32 = new QStandardItem(QString("addr:port")); itemESP32->appendRow(tarESP32);
		itemESP32->setChild(tarESP32->index().row(), 1, new QStandardItem(it->addr.toString() + QString().sprintf(":%d", it->port)));
		QStandardItem* connESP32 = new QStandardItem(QString("connection")); itemESP32->appendRow(connESP32);
		itemESP32->setChild(connESP32->index().row(), 1, new QStandardItem(QString(it->socket.state() == QAbstractSocket::ConnectedState ? "true" : "false")));
		// binded GPIOext
		for (auto bg = gpioexts.begin(); bg != gpioexts.end(); ++bg) {
			if (bg->bindType != "ESP32" || bg->bindId != it->id) continue;
			QStandardItem* itemGPext = new QStandardItem(QString("GPIOExt")); itemESP32->appendRow(itemGPext);
			itemESP32->setChild(itemGPext->index().row(), 1, new QStandardItem(QString().sprintf("[%d]", bg->id)));
			QStandardItem* bingmethodGPext = new QStandardItem(QString("bindmethod")); itemGPext->appendRow(bingmethodGPext);
			itemGPext->setChild(bingmethodGPext->index().row(), 1, new QStandardItem(bg->bindMethod));
		}
		// binded plugins
		for (auto bp = plugins.begin(); bp != plugins.end(); ++bp) {
			if (bp->bindType != "ESP32" || bp->bindId != it->id) continue;
			QStandardItem* itemPlugin = new QStandardItem(QString("Plugin")); itemESP32->appendRow(itemPlugin);
			itemESP32->setChild(itemPlugin->index().row(), 1, new QStandardItem(QString().sprintf("<%d, %d>", bp->position.alpha, bp->position.beta)));
			QStandardItem* namePlugin = new QStandardItem(QString("name")); itemPlugin->appendRow(namePlugin);
			itemPlugin->setChild(namePlugin->index().row(), 1, new QStandardItem(bp->name));
			QStandardItem* bingmethodPlugin = new QStandardItem(QString("bindmethod")); itemPlugin->appendRow(bingmethodPlugin);
			itemPlugin->setChild(bingmethodPlugin->index().row(), 1, new QStandardItem(bp->bindMethod));
		}
	}
	treeView_ESP32s->setModel(model);
	treeView_ESP32s->expandAll();
	treeView_ESP32s->setColumnWidth(0, treeView_ESP32s->width() / 2);
}

double IntPoint::y() const
{
	return alpha + ((double)beta) / 2;
}

double IntPoint::x() const
{
	return ((double)beta) / 2 * sqrt(3);
}

bool IntPoint::operator<(const IntPoint & p) const
{
	if (alpha < p.alpha) return true;
	if (alpha > p.alpha) return false;
	if (beta < p.beta) return true;
	return false;
}

vector<double> Switch3Line::giveme3line() const
{
	vector<double> pl; // Ax1, Ay1, Ax2, Ay2, Bx1, By1, Bx2, By2, Cx1, Cy1, Cx2, Cy2, 12 elements
	double ax = a.x(), ay = a.y();
	double bx = b.x(), by = b.y();
	double deltaX = gap3line * (ay - by);
	double deltaY = gap3line * (bx - ax);
	// compute
	double x1 = ax + gapcenterline * (bx - ax);
	double y1 = ay + gapcenterline * (by - ay);
	double x2 = bx + gapcenterline * (ax - bx);
	double y2 = by + gapcenterline * (ay - by);

	// left
	pl.push_back(x1 + deltaX);
	pl.push_back(y1 + deltaY);
	pl.push_back(x2 + deltaX);
	pl.push_back(y2 + deltaY);

	// middle
	pl.push_back(x1);
	pl.push_back(y1);
	pl.push_back(x2);
	pl.push_back(y2);

	// right
	pl.push_back(x1 - deltaX);
	pl.push_back(y1 - deltaY);
	pl.push_back(x2 - deltaX);
	pl.push_back(y2 - deltaY);
	return pl;
}

QColor Switch3Line::getColor(int lineno) const
{
	if (lineno < 0 || lineno > 2) {
		SDRprintf("error %s(%d): lineno = %d\n", __FILE__, __LINE__, lineno);
		return QColor("red");
	}
	if (lineStates[lineno] == 0) {
		return QColor("grey");
	}
	if (lineStates[lineno] == 1) {
		return QColor("blue");
	}
	else {
		SDRerror("lineStates[lineno] = %d", lineStates[lineno]);
		return QColor("red");
	}
}

bool Switch3Line::operator<(const Switch3Line & l) const
{
	const IntPoint minp1 = a < b ? a : b;
	const IntPoint maxp1 = a < b ? b : a;
	const IntPoint minp2 = l.a < l.b ? l.a : l.b;
	const IntPoint maxp2 = l.a < l.b ? l.b : l.a;
	if (minp1 < minp2) return true;
	if (minp2 < minp1) return false;
	if (maxp1 < maxp2) return true;
	return false;
}

bool PluginDevice::operator<(const PluginDevice & p) const
{
	return position < p.position;
}

QColor PluginDevice::getColor() const
{
	if (state == 0) {
		return QColor("green");
	}
	if (state == 1) {
		return QColor("yellow");
	}
	SDRerror("state = %d", state);
	return QColor("red");
}

bool ESP32::operator<(const ESP32 & x) const
{
	if (addr.toString() < x.addr.toString()) return true;
	if (addr.toString() > x.addr.toString()) return false;
	if (port < x.port) return true;
	return false;
}

bool GPIOExt::operator<(const GPIOExt & p) const
{
	return id < p.id;
}
