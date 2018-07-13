PIN_PIR_LED       D0   Led estado Pir 330 ohm a masa(onboard)
PIN_MANIOBRA      D1   Puerta1   peatonal  5 segundos   activo nivel bajo
PIN_MANIOBRA2     D2   Puerta2                          activo nivel bajo
PIN_LUZ           D3   Luz On Off                       activo nivel bajo
PIN_AUX           D4   Aux On Off                       activo nivel bajo
PIN_IR            D5   Emisor IR
PIN_PIR_IN        D6   Entrada sensor Pir activo nivel alto
PIN_SENSOR_C      D7   Entarda sensor Puerta Cerrada (NC)  10k a masa Activo +3.3
                  D8
PIN_DS18          RX   Entrada sensor Temperatura DS18B20
PIN_NTC           A0   Entrada sensor temperatura Ntc 10k entre A0 y masa + Resistencia de 100k entre A0 Y 3.3v
WiFi_CONFID       Doble Reset   Configuracion WiFi 

Funciones disponibles.
/Puerta= Maniobra   D1 0,5 segundos
/PP= Paso Peatonal  D1 0,5 segundos pausa 5 segundos D1 0,5 segundos
/Puerta2= Maniobra2 D2 0,5 segundos
/Pir : Conecta o Desconecta el Sensor de Movimiento
/AireC : Enciende el AA en Calor   emisor IR en pin D5 
/AireF : Enciende el AA en Frio    emisor IR en pin D5 
/AireOff : Apaga el AA             emisor IR en pin D5 
/Aux : Aux 0-1   D4 conectado/desconectado
/Luz : Luz 0-1   D3 conectado/desconectado
/Temp : Temperatura Ambiente
/ip : ip publica   responde con la ip publica del dispositivo
/Estado : Indica el estado de Puerta,Sensor de Movimiento,Luz,Etc.
/tecla : Teclado