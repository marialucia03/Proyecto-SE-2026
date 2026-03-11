# SISTEMA DE PREVENCIÓN DE SOBRECALENTAMIENTO PARA DEPORTISTAS

## Integrantes

- María Lucia Quintanilla Álvarez (Technical Lead)
- Anna Posada Portilla (Verification & Testing Engineer)
- Samuel Mercado Mercado (Firmware Engineer)
- Martín Pérez Betancourt (Hardware Integration Engineer)

## Proyecto

Durante la actividad física intensa, el organismo experimenta un incremento sostenido de la temperatura corporal y la frecuencia cardíaca, producto de la mayor demanda metabólica. Cuando estas variables sobrepasan determinados umbrales fisiológicos, pueden desencadenarse estados de estrés térmico que comprometen el rendimiento del deportista y, en casos extremos, riesgos para la salud tales como deshidratación o golpe de calor. La detección oportuna de estas condiciones, resulta fundamental para prevenir episodios de sobrecalentamiento durante el ejercicio. 

En la actualidad existen dispositivos capaces de monitorear variables fisiológicas como la frecuencia cardíaca y la temperatura corporal. Ejemplos de estas tecnologías incluyen relojes inteligentes y aplicaciones de monitoreo deportivo como Garmin Connect®, Strava® o Apple Health®, así como dispositivos wearables como el Apple Watch®. Sin embargo, su funcionalidad suele limitarse al monitoreo y análisis posterior de datos. En algunos casos, emiten alguna alarmas automática cuando se superan ciertos valores, pero generalmente dejan al usuario la responsabilidad de interpretar la información y tomar decisiones para evitar situaciones de riesgo, sin incorporar mecanismos automáticos de respuesta que contribuyan a mitigar dichas condiciones.  

En este contexto, el presente proyecto propone el desarrollo de un sistema embebido capaz de monitorear en tiempo real la temperatura superficial de la piel y la frecuencia cardíaca de un deportista durante la actividad física. A partir de dichas mediciones, el sistema evaluará el estado fisiológico del usuario y determinará si se presentan condiciones asociadas a un posible sobrecalentamiento corporal. Con base a este análisis, se activará automáticamente un mecanismo de enfriamiento mediante una microbomba, encargada de hacer circular líquido refrigerante a través de un sistema de enfriamiento portátil. Adicionalmente, el sistema registrará los eventos relevantes de funcionamiento y permitirá visualizar su estado operativo por medio de una interfaz de usuario. 


**Objetivo General:**
Desarrollar un prototipo de sistema embebido capaz de monitorear variables fisiológicas básicas, específicamente temperatura superficial de la piel y frecuencia cardíaca, con el fin de detectar condiciones asociadas al sobrecalentamiento durante la actividad física y activar un mecanismo de enfriamiento simulado como respuesta automática.

**Objetivos Específicos**
1. Definir los componentes del hardware necesarios, incluyecto sensores fisiológicos, microcontroladores y los actuadores necesarios.
2. Implementar un firmware encargdo de la adquisición, procesamiento y evaluación de datos sobre temperatura y frecuecnia cardiaca.
3. Establecer umbrales de operación con base a condiciones de sobrecalentamiento y variables fisiológicas. 
4. Integrar un actuador de enfriamento controlado que se active automáticamente al detectar condiciones de riesgo.
5. Realizar pruebas del prototipo bajo condiciones simuladas con un sujeto de prueba humano.
6. Verificar el correcto funcionamiento del sistema y de la interfaz de usuario, permitiendo visualizar las variables fisiológicas monitoreadas, las condiciones de operación del sistema y la activación del mecanismo de enfriamiento cuando se superen los umbrales establecidos.
