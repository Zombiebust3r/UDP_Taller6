#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

int pos;
sf::UdpSocket sock;

//HACER UN SISTEMA PARA ENVIO HASTA CONFIRMACIÓN:
	//UNA FUNCIÓN QUE SE LLAME EN EL RECEIVE QUE ELIMINE EL MENSAJE CONFIRMADO DE LA LISTA DE PENDIENTES. SE EJECUTA ANTES DEL ENVIO PARA NO ENVIAR ALGO YA CONFIRMADO.
	//UNA FUNCIÓN QUE PASE POR UN VECTOR DE MESNAJES (ID, MSG, TIEMPO [se añade dt cada frame]) <int, packet, int(milisegundos)> Y LOS ENVIE CADA 500ms.

void DibujaSFML()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Sin acumulación en cliente");

	while (window.isOpen())
	{
		sf::Event event;

		//INICIAR CHRONO PARA DELTATIME ----------------------------------------------

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
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos)

				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					sf::Packet pckRight;
					int posAux = pos + 1;
					pckRight << posAux;
					sock.send(pckRight, IP_SERVER, PORT_SERVER);
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos)
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
			//Gestionar si se conecta un nuevo jugador con prefijo NEW en el packet.
			//Gestionar si un jugador se mueve: Recibimos su ID y su nueva pos como dos ints (x, y) por separado con un prefijo MOV en el packet.
			pck >> pos;
			std::cout << "Recibo la confirmacion: " << pos << std::endl;
		}

		//REENVIAR PENDIENTES CADA 500ms.
			//for()
				//if(msg[i].time > 500ms) { reenviar + resetear a 0 el time }
				//else { time + chronoDeltaTime.getTime().asMilliseconds(); }
			
			//chronoDeltaTime.reset();

		window.clear();

		sf::RectangleShape rectBlanco(sf::Vector2f(1, 600));
		rectBlanco.setFillColor(sf::Color::White);
		rectBlanco.setPosition(sf::Vector2f(200, 0));
		window.draw(rectBlanco);
		rectBlanco.setPosition(sf::Vector2f(600, 0));
		window.draw(rectBlanco);

		//For para cada jugador -> rectAvatar(players[i].pos) | rectAvatar(sf::Vector2f(players[i].pos.x, players[i].pos.y))
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