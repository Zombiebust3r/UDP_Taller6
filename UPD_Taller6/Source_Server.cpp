#include <SFML\Network.hpp>
#include <iostream>

int main()
{
	sf::UdpSocket sock;
	sf::Socket::Status status = sock.bind(50000);
	if (status != sf::Socket::Done)
	{
		std::cout << "Error al vincular\n";
		system("pause");
		exit(0);
	}
	sf::Clock clock;
	do
	{
		if (clock.getElapsedTime().asMilliseconds() > 200)
		{
			sf::Packet pck;
			sf::IpAddress ipRem; unsigned short portRem;
			status = sock.receive(pck, ipRem, portRem);
			if (status == sf::Socket::Done)
			{
				int pos;
				pck >> pos;
				if (pos == -1)
				{
					break;
				}

				std::cout << "Se intenta la pos " << pos << std::endl;
				if (pos >= 200 && pos <= 600)
				{
					std::cout << "Se confirma la pos " << pos << std::endl;
					sf::Packet pckSend;
					pckSend << pos;
					sock.send(pckSend, ipRem, portRem);

				}
			}
			clock.restart();
		}
	} while (true);
	sock.unbind();
	return 0;
}