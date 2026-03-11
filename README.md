# SISTEMA DE PREVENCIÓN DE SOBRECALENTAMIENTO PARA DEPORTISTAS

## Integrantes

- María Lucia Quintanilla Álvarez (Technical Lead)
- Anna Posada Portilla (Verification & Testing Engineer)
- Samuel Mercado Mercado (Firmware Engineer)
- Martín Pérez Betancourt (Hardware Integration Engineer)

## Proyecto

Durante la actividad física intensa, el organismo experimenta un incremento sostenido de la temperatura corporal y la frecuencia cardíaca, producto de la mayor demanda metabólica. Cuando estas variables sobrepasan determinados umbrales fisiológicos, pueden desencadenarse estados de estrés térmico que comprometen el rendimiento del deportista y, en casos extremos, derivan en riesgos para la salud tales como deshidratación o golpe de calor. La detección oportuna de estas condiciones resulta, por tanto, fundamental para prevenir episodios de sobrecalentamiento durante el ejercicio. 

Si bien en la actualidad existen dispositivos capaces de monitorear variables fisiológicas como la frecuencia cardíaca o la temperatura corporal, gran parte de estos sistemas se limita a presentar información al usuario sin incorporar mecanismos automáticos de respuesta que contribuyan a mitigar los riesgos asociados a dichas condiciones.  

En este contexto, el presente proyecto propone el desarrollo de un sistema embebido capaz de monitorear en tiempo real la temperatura superficial de la piel y la frecuencia cardíaca de un deportista durante la actividad física. A partir de dichas mediciones, el sistema evaluará el estado fisiológico del usuario y determinará si se presentan condiciones asociadas a un posible sobrecalentamiento corporal. Ante la detección de tales condiciones, se activará automáticamente un mecanismo de enfriamiento mediante una microbomba, encargada de hacer circular líquido refrigerante a través de un sistema de enfriamiento portátil. Adicionalmente, el sistema registrará los eventos relevantes de funcionamiento y permitirá visualizar su estado operativo por medio de una interfaz de usuario. 

El desarrollo del proyecto incluye el diseño de la arquitectura de hardware y firmware, la integración de sensores fisiológicos, la implementación del control del actuador de enfriamiento y la validación del prototipo. El sistema se plantea como un prototipo experimental orientado a demostrar el funcionamiento de un sistema embebido aplicado al monitoreo fisiológico y la prevención de sobrecalentamiento durante la actividad física. 

# Objetivo general
Desarrollar un prototipo de sistema embebido capaz de monitorear variables fisiológicas básicas, específicamente la temperatura superficial de la piel y la frecuencia cardíaca, con el fin de detectar condiciones asociadas al sobrecalentamiento durante la actividad física y activar automáticamente un mecanismo de enfriamiento como respuesta.

# Objetivos específicos 
1. Definir los componentes de hardware necesarios, incluyendo sensores fisiológicos, microcontrolador y actuadores requeridos para el sistema de enfriamiento.

2. Desarrollar e implementar el firmware encargado de la adquisición, procesamiento y evaluación de los datos provenientes de los sensores de temperatura de piel y frecuencia cardíaca.

3. Establecer umbrales de operación basados en condiciones fisiológicas asociadas a sobrecalentamiento durante la actividad física.

4. Integrar un actuador de enfriamiento controlado electrónicamente que se active automáticamente cuando se detecten condiciones de riesgo.

5. Realizar pruebas experimentales del prototipo bajo condiciones simuladas de ejercicio utilizando un sujeto de prueba humano.

6. Verificar el correcto funcionamiento del sistema y de su interfaz de usuario, comprobando que esta permita visualizar las variables fisiológicas monitoreadas, las condiciones de operación y la activación del mecanismo de enfriamiento ante la superación de los umbrales establecidos.
