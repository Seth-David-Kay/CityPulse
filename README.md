# Impulse

## Run
- compile
	- ./build.sh from base dir for mac
- run
	- build/capstone.bin
- or use ./run.sh (osX)

- test based development
- ticket based development
	- description, assignee, epic, due date, sprint, status
	- all tickets should be done, won't do, or moved to the next sprint


# NO AI USED IN CREATION OF IMPULSE, ONLY AI CREATED


# Next:

path finding –> vehicle simulation across the path -> mutli-car-aware vehicle simulation across the path


# Setup:
I'm thinking: (Seth David)

cars are not reinforceement learning, they are a model of the world. Cars will have a destination, a bunch of other things. they feed into the visualization a direction as a degree where 0/360 is due N, and 90 is due E, etc., and a speed (which can be converted into how far thehy move in this time step). Collisions in cars can be calculated, we know the force and the direction on teh force so we can use some physics to understand the force applied, and can use a car crash survivability database to understand if a car is hit with some amount of force and some angle at some part of the car, the chagne of survival/amount of physical damage to the car/person(s) in it.

The reinforcement learning comes in for now with the most basic implementation I can think of : trafic lights. They will turn based on the continous space they are in given evberything we know about the environment (this can change idk what will be useful for them to have), and they will be rewarded for minimizing damage to people/cars (people weighed much higher), as well as the throughput of cars during the simulation time (factor in the amount of cars that come in too, as spwawning cars will be somewhat random).

Nothing will be pre-built, we will input a street design and define all possible points cars can come and go from as well as wehre traffic lights are, and let everything run its course. We obviously also have to define where cars can collide with the environment and where they can't.

Cars will have a chance at certain attributes of the driver and car. For example, car weight and size, bound by a limit. The same will go for the driver. For example, take a car behind a car, where the car in front is turning. The car behind will have maybe a 10% chance (find real world numbers from studies) at not seeing the turn signal every second. So the further away the signal is turned, the higher the chance it is seen by the car in back. This should be subject to change bound by limits, so a car could have a higher or lower chance at seeing the signal depending on how skilled or not distracted the driver of the car is. Therefore, some variables logically move together, like reaction time (swerving) and seeing a turn signal (slowing down). These variables should generally move together because if a driver is more skilled or not distracted it stands to reason they would be more apt at dealing with both situations.

Anyway those are my thoughts so far...

https://docs.google.com/document/d/1ee1mF3l6n7QIdhe1-7H2-61El8cK8Bo58oJ3YzfEmdI/edit?usp=sharing
