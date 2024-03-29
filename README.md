<h1>Electric Vehicle Charging Grid Simulation using Distributed Wireless Sensor Network</h1>

This project aims to simulate a Wireless Sensor Network (WSN) of interconnected Electric Vehicle (EV) charging nodes using **C**, incorporating both **Message Passing Interface (MPI) and POSIX threads** for parallel processing. Charging nodes and the base station are represented as MPI processes.   

<h2>Overview</h2>
The wireless sensor network (WSN) consists of <em> m x n </em> nodes in a cartesian grid, and a base station, forming a comprehensive Electric Vehicle (EV) charging infrastructure.       

&nbsp; 
      
**1. Charging Node**    
Each node emulates the behaviour of the EV charging node, and each node has <em> k </em> numbers of in use or free charging ports. Each node is only able to communicate and exchange data, specifically the number of available ports, with its adjacent nodes. Most importantly, each charging node can exchange data with the base station independently.    

**2. Base Station**   
The base station is responsible for receiving reports, logging data, and providing information about available charging nodes to reporting nodes. Additionally, it issues a termination message to EV charging nodes for proper shutdown during maintenance.

&nbsp; 

![image](https://github.com/leongyenni/ev-charging-grid/assets/75636975/b884fc26-5f26-4464-9168-e1b82d232df8)   
<p align="center"> <em>Fig 1. An example of a 3x3 EV charging node grid in WSN </em> </p>

&nbsp; 

![image](https://github.com/leongyenni/ev-charging-grid/assets/75636975/0fe11b3e-82c4-455b-8f1b-82dd08341d1d)    
<p align="center"> <em>Fig 2. Message exchange for each communication </em> </p>

<h2>Architecture Design</h2>

![FIT3143 EV Charging Station - Flow chart](https://github.com/leongyenni/ev-charging-grid/assets/75636975/27119bb9-78db-4bfb-9655-e8ece16ec387)
<p align="center"> <em>Fig 3. Flowchart of the WSN </em> </p>


<h2>Logging Mechanism</h2>

![image](https://github.com/leongyenni/ev-charging-grid/assets/75636975/20f59f31-75d3-4b0f-b450-203ef730bbe7)   
<p align="center"> <em>Fig 4. Screenshot of logging text file </em> </p>


