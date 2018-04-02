#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

int pos;
sf::UdpSocket sock;

void DibujaSFML()
{

	sf::RenderWindow window(sf::VideoMode(800, 600), "Sin acumulación en cliente");

	while (window.isOpen())
	{
		sf::Event event;

		//Este primer WHILE es para controlar los eventos del mouse
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::KeyPressed:
				if (event.key.code == sf::Keyboard::Left)
				{
					sf::Packet pckLeft;
					int posAux = pos - 1;
					pckLeft << posAux;
					sock.send(pckLeft, IP_SERVER, PORT_SERVER);

				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					sf::Packet pckRight;
					int posAux = pos + 1;
					pckRight << posAux;
					sock.send(pckRight, IP_SERVER, PORT_SERVER);
				}
				break;

			default:
				break;

			}
		}
		
		sf::Packet pck;
		sf::IpAddress ipRem;
		unsigned short portRem;
		sf::Socket::Status status = sock.receive(pck, ipRem, portRem);
		if (status == sf::Socket::Done)
		{
			pck >> pos;
			std::cout << "Recibo la confirmacion: " << pos << std::endl;
		}

		window.clear();

		sf::RectangleShape rectBlanco(sf::Vector2f(1, 600));
		rectBlanco.setFillColor(sf::Color::White);
		rectBlanco.setPosition(sf::Vector2f(200, 0));
		window.draw(rectBlanco);
		rectBlanco.setPosition(sf::Vector2f(600, 0));
		window.draw(rectBlanco);

		sf::RectangleShape rectAvatar(sf::Vector2f(60, 60));
		rectAvatar.setFillColor(sf::Color::Green);
		rectAvatar.setPosition(sf::Vector2f(pos, 280));
		window.draw(rectAvatar);

		


		

		window.display();
	}

}

int main()
{
	pos = 200;
	sock.setBlocking(false);
	DibujaSFML();
	return 0;
}