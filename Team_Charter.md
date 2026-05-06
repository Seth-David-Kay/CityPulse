# Team Charter
Last updated: 2025-10-04

## Team Name: CityPulse

### Team Members:
- Nawwaf Aleisa
- Seth David Kay
- Felix Ulmanu

## 1. Project Summary

CityPulse is a city-scale traffic simulation platform that uses Multi-Agent Reinforcement Learning (MARL) to control traffic signals in a way that optimizes traffic flow, minimizes congestion, and adapts to complex road structures. Our system leverages reinforcement learning (RL), a branch of artificial intelligence where agents learn optimal behaviour through trial and error, to train autonomous traffic light controllers. Our simulation is entirely custom-built in C, without relying on external ML libraries. This gives us maximum performance and transparency, as well as low-level control of traffic simulation, agent policy, and rendering using a modular architecture. CityPulse’s rigorous technical implementation gives full visibility into every layer of the system: from agent decision logic, to the traffic simulation engine, to rendering and interactivity.

Urban traffic congestion is a persistent global challenge. It leads to significant time loss, fuel consumption, and environmental degradation. Traffic light timings are one element impacting congestion and safety in urban environments. Traditional traffic light systems use fixed timers or centralized coordination, which lack the flexibility to adapt to dynamic, real-traffic patterns. Existing smart traffic systems often require centralized data collection or rely on idealized grid layouts, limiting their applicability to the irregular road networks seen in real cities. Our system addresses this by simulating irregular intersection layouts and using agents that act independently with no inter-agent communication, creating a testbed for decentralized coordination under realistic constraints.

The target audience includes urban mobility researchers, smart city infrastructure teams, transportation engineers, and academic institutions interested in reinforcement learning or traffic optimization. Our users likely have a background in transportation, AI, or policy planning. Users can analyze agent performance, modify learning policies, or simulate their own maps. The inclusion of a physics engine for realistic car behaviour and a graphical UI for monitoring traffic states makes the platform both educational and research-capable.

CityPulse introduces several key innovations. First, we support non-grid urban layouts, allowing the simulation of real-world city geometries. Second, we implement Proximal Policy Optimization (PPO) — a powerful RL algorithm — from scratch enabling full control and extensibility. Third, we adopt a decentralized control paradigm, with each traffic light functioning as a standalone agent. This contrasts with prior approaches that assume global knowledge or rely on complex coordination mechanisms. By forcing each agent to make decisions based solely on local observations, we explore how intelligent policies can emerge from bottom-up learning, a crucial property for scalable urban systems.

Our broader societal impact is to democratize experimentation with smart traffic systems and provide a realistic platform for studying how AI can support urban infrastructure. As global urbanization accelerates, it is crucial to design systems that can adapt and scale without centralized failure points. CityPulse empowers researchers and students to experiment with these challenges in a controlled but realistic environment, contributing to both educational and technological advancement.

CityPulse is an original and technically ambitious project that combines algorithmic learning, systems programming, and real-world impact. By building every component from scratch and focusing on unique constraints like irregular map geometry and decentralization, we aim to produce a simulator and agents that push the boundaries of traffic research and pave the way for smarter, more resilient urban mobility systems.

## 2. Project Objectives

Our project will be an AI-Driven multi-agent traffic signal control system built from scratch in C, using reinforcement learning to optimize traffic in irregular urban road networks. We will be implementing a multi-agent reinforcement learning model using Proximal Policy Optimization with individual and centralized loss prediction for traffic light timing optimization. To train and test, we will implement an advanced traffic and individual car physics and behavioral simulation. Our interactive, real-time, graphical program will be implemented from scratch in C (with GPU assistance via compute shaders) in order to provide a responsive, accessible experience that encourages discovery and experimentation.

### 2.1 Specific Goals

- Proximal Policy Optimization implementation: Highly optimized for our problem with our custom implementation, create both the actor and critic neural networks for individual traffic lights, and a critic for the system as a whole.
- Vehicular simulation which accurately models real-world traffic behaviour, acting as training data
- All code in C, with GPU compute shaders for reinforcement learning (and for vehicular simulation if necessary)
- Interactive, intuitive interface which allows users to load urban layouts from real addresses via the OpenStreetMaps API

## 3. Roles and Responsibilities

### 3.1 Team Lead

Nawwaf Aleisa is the project lead, helping us organise and stick to deadlines. Seth David Kay is managing the GitHub Project Board. Felix Ulmanu is taking meeting notes and posting them in the team slack channel. We are all responsible for coordinating meeting times and discussions.

### 3.2 Member Roles

| Member | Strengths | Responsibilities |
| --------- | ------------ | ------------------ |
| Nawwaf Aleisa | ML/RL, Algorithms, C, Debugging, Data structures, Git  | PPO Training, Agent logic, neural network, policy networks, reinforcement learning agent architecture |
| Seth David Kay | ML/RL, OOP, Advanced Algorithms, Algorithm Optimization, C | PPO Implementation & Optimizations, Traffic Simulation |
| Felix Ulmanu | C, UI, UX, GPU programming, optimisation | Build interface & graphical features, optimise simulation & RL code |

### 3.3 Logistics

- Nawwaf Aleisa: helps plan and stay on top of deadlines; researches and trains our RL stack
- Seth David Kay: keeps track of GitHub project board; implements traffic simulation
- Felix Ulmanu: takes notes during instructor & mentor meetings and posts them to slack

## 4. Communication Guidelines

### 4.1 Communication Tools

- We will use Slack for async communication between team members, mentors, and instructors
- We will use Zoom for weekly meetings with instructors and mentors
- We will use GitHub projects to track sprint progress and post weekly updates

### 4.2 Meetings

- Weekly meetings with the team mentor will be held *Tuesday 6:30pm* over Zoom.
- Weekly meetings with the instructor will be help *Tuesday 5pm* over Zoom
- Weekly meetings will be on Monday at 3:30pm, at a flexible time on weekends, and at various other times when necessary

### 4.3 Documentation & Reporting

- We will document and share our meeting notes over Slack within 24 hours of each meeting.
- We will post weekly status updates to github projects no later than Sunday night outlining:
	- what they worked on over the past week
	- if they are blocked on anything
	- what they are currently working on
- All members will be given equal opportunity to participate in discussion and be open to listening to diverse perspectives on all topics/ideas.
- All members must be transparent about their progress, challenges, and any delays they anticipate.

## 5. Decision-Making Process

- **Consensus-Based Decisions**: Major decisions will be made through consensus among all team members. In case of a deadlock, a majority vote will have the final say. If no majority is reached, the team will reach out to their mentor and instructor for guidance.
- **Documentation**: Decisions relevant to other members must be documented and shared with the team to ensure clarity and alignment.
- **Conflict Resolution**: Any conflicts between team members should first be addressed internally through open communication. If unresolved, members must notify the instructor who will then mediate.

## 6. Performance Standards

### 6.1 Work Distribution
- **Flexibility**: If a member is unable to complete a task, they must notify us as soon as possible to allow for reallocation or assistance.

### 6.2 Respect and Professionalism

- **Mutual Respect**: All members are expected to treat each other with respect and professionalism.
- **Constructive Feedback**: Feedback should be given and received constructively, focusing on improving the project.

## 7. Resource Allocation

- **Time Commitment**: All members are expected to spend ~10 hours of time outside of class per week on the project.
- **Workload Distribution**: We will assign tasks based on each member's expertise and availability. We expect each member to complete their assigned tasks within the agreed-upon deadlines.
- **Shared Resources:**: We are each using our own hardware and our code is cross-platform. We do not need any server infrastructure.

## 8. AI Use

We will not be using AI.

## 9. Signatures

By signing below, each team member agrees to uphold this team charter (add names as separate commits):
- Seth David Kay , 10/04/2025
- Nawwaf Abdulaziz Aleisa , 10/04/2025
- Felix Ulmanu, 2025-10-04

## 10. Charter Review and Updates

This charter was last updated on 2025-10-04.
It was previously modified on:

- 2025-10-04
