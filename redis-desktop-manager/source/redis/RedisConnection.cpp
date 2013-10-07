#include "RedisConnection.h"

RedisConnection::RedisConnection(const RedisConnectionConfig & c) 
	: RedisConnectionAbstract(c)
{
	socket = new QTcpSocket();
}

RedisConnection::~RedisConnection()
{
	if (connected) {
		socket->disconnectFromHost();
	}

	delete socket;
}


bool RedisConnection::connect()
{
	socket->connectToHost(config.host, config.port);

	if (socket->waitForConnected(config.connectionTimeout)) 
	{
		connected = true;

		if (config.useAuth()) {
			execute(QString("AUTH %1").arg(config.auth));
		}
	} else {
		connected = false;
	}

	return connected;
}


QString RedisConnection::getLastError()
{
	return socket->errorString();
}

QVariant RedisConnection::execute(QString command)
{		
	if (command.isEmpty()) {
		return QVariant();
	}

	QString formattedCommand = prepareCommand(command);

	/*
	 *	Send command
	 */
	QTextStream out(socket);
	out << formattedCommand;
	out.flush();

	if (!socket->waitForReadyRead(config.executeTimeout)) {

		QAbstractSocket::SocketError error = socket->error();

		if (error == QAbstractSocket::UnknownSocketError && connect()) {
			return execute(command);
		}

		return QVariant();
	}

	/*
	 *	Get response
	 */	
	QString response("");		
	unsigned int lastBytesAvailable = 0, currBytesAvailable = 0;

	while(!isFullResponseRecieved(response)) {
		
		currBytesAvailable= socket->bytesAvailable();

		if (currBytesAvailable > lastBytesAvailable) {
			lastBytesAvailable = currBytesAvailable;
			continue;
		}

		if (!socket->atEnd()) 
		{
			response.append(socket->readAll());	

		} else {
			
			// TODO: move config.executeTimeout to config options - user probably want to increase this value for unstable connections			

			if (waitForData(5) && socket->canReadLine()) continue;			

			if (waitForData(50) && socket->canReadLine()) continue;			

			if (waitForData(100) && socket->canReadLine()) continue;

			if (waitForData(config.executeTimeout) && socket->canReadLine()) continue;

			break;
		}

	}	

	return parseResponse(response);
}


