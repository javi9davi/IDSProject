# IDSProject

Sistema de detección de intrusiones (IDS) diseñado para entornos virtualizados basados en KVM, proporcionando monitorización continua, análisis dinámico y respuesta ante eventos sospechosos en máquinas virtuales gestionadas mediante libvirt.

## Descripción

El IDS propuesto permite supervisar múltiples máquinas virtuales mediante técnicas de introspección basadas en la biblioteca [libvirt](https://libvirt.org/), y consultas externas de reputación de archivos mediante [MalwareBazaar](https://bazaar.abuse.ch/api/). El sistema ofrece:
* Monitorización de procesos y detección de anomalías
* Comprobación periódica de integridad de archivos mediante hashing SHA-256
* Análisis en tiempo real de tráfico de red capturado vía tcpdump
* Integración con interfaz gráfica en Qt

## Características principales

* **Monitorización**: Uso del agente QEMU para interacción con las VMs.
* **Escalabilidad**: Soporte para múltiples máquinas virtuales simultáneas.
* **Flexibilidad**: Configuración sencilla mediante archivo JSON y variables de entorno.
* **Seguridad**: Alertas inmediatas ante eventos sospechosos como accesos no autorizados o ejecución de binarios maliciosos.

## Tecnologías empleadas

* **KVM** y **libvirt**: Gestión y monitorización del entorno virtualizado.
* **LibVMI**: Introspección no intrusiva en memoria.
* **Qt6**: Interfaz gráfica para monitorización y gestión.
* **SQLite**: Almacenamiento local en caché de resultados de consultas a MalwareBazaar.
* **C++**: Desarrollo del backend y módulos de monitorización.

## Estructura del proyecto

```
IDSProject
├── src
│   ├── backend
│   ├── monitor
│   │   ├── fileMonitor
│   │   ├── processMonitor
│   │   └── networkMonitor
│   └── ui
├── config
│   ├── config.json
│   └── .env
└── cache
```

## Requisitos previos

* Distribución Linux con soporte KVM y libvirt (recomendado Ubuntu 22.04 LTS).
* Paquetes: libvirt-dev, libvmi-dev, SQLite, Qt6, tcpdump, OpenSSL.

## Instalación

1. Clonar el repositorio

```bash
git clone https://github.com/javi9davi/IDSProject.git
cd IDSProject
```

2. Instalar dependencias

```bash
sudo apt install build-essential cmake libvirt-dev libvmi-dev libsqlite3-dev libssl-dev qt6-base-dev tcpdump
```

3. Compilar el proyecto

```bash
mkdir build && cd build
cmake ..
make
```

4. Configurar las variables necesarias en `config/.env`

```env
API_KEY=tu_clave_de_MalwareBazaar
```

> La clave API debe obtenerse registrándose en el portal [MalwareBazaar](https://bazaar.abuse.ch/api/). Esta clave se usa para consultar la reputación de hashes de archivos sospechosos.

## Ejecución

```bash
./IDSProject
```
