/* Copyright (c) <2003-2022> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndSkyBox.h"
#include "ndUIEntity.h"
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndMakeStaticMap.h"
#include "ndDemoEntityManager.h"
#include "ndDemoInstanceEntity.h"

namespace ndController_1
{
	//#define ND_ALPHA_TOL ndFloat32 (1.0e-3f)
	#define ND_ACTIONS			12
	#define ND_TOTAL_ACTIONS	(2 * ND_ACTIONS + 1)

	class ndModelUnicycle : public ndModelArticulation
	{
		public:
		ndModelUnicycle()
			:ndModelArticulation()
			//,m_invInertia(ndGetZeroMatrix())
			//,m_com(ndVector::m_zero)
			//,m_comVel(ndVector::m_zero)
			//,m_gyroTorque(ndVector::m_zero)
			//,m_comDist()
			,m_bodies()
			,m_ballBody(nullptr)
			,m_controlJoint(nullptr)
			,m_invMass(ndFloat32 (0.0f))
		{
			ndFloat32 angleStep = ndFloat32(0.25f) * ndDegreeToRad;
			for (ndInt32 i = 0; i < ND_TOTAL_ACTIONS; ++i)
			{
				m_actionMap[i * 2 + 1] = ndReal(-angleStep);
				m_actionMap[i * 2 + 2] = ndReal(angleStep);
				angleStep *= ndFloat32(1.5f);
			}
			m_actionMap[0] = 0.0f;
		}

		void Init()
		{
			ndFloat32 mass = ndFloat32(0.0f);
			for (ndNode* node = m_rootNode->GetFirstIterator(); node; node = node->GetNextIterator())
			{
				ndBodyDynamic* const body = node->m_body->GetAsBodyDynamic();
				m_bodies.PushBack(body);
				mass += body->GetMassMatrix().m_w;
			}
			m_invMass = ndFloat32(1.0f) / mass;
			//m_comDist.SetCount(m_bodies.GetCount());
		}

		bool ValidateContact(ndWorld* const world)
		{
			bool hasSupport = false;
			ndBodyKinematic::ndContactMap::Iterator it(m_ballBody->GetContactMap());
			for (it.Begin(); it; it++)
			{
				ndContact* const contact = it.GetNode()->GetInfo();
				if (contact->IsActive())
				{
					world->CalculateJointContacts(contact);
					hasSupport = true;
				}
			}
			return hasSupport;
		}

#if 0
		void InitState(ndWorld* const world)
		{
			//a) Mt = sum(m(i))
			//b) cg = sum(p(i) * m(i)) / Mt
			//c) Vcg = sum(v(i) * m(i)) / Mt
			//d) Icg = sum(I(i) + covarianMatrix(p(i) - cg) * m(i))
			//e) T0 = sum[w(i) x (I(i) * w(i)) - Vcg x (m(i) * v(i))]
			//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
			//g) Bcg = (Icg ^ -1) * (T0 + T1)

			//a) Mt = sum(m(i))
			//b) cg = sum(p(i) * m(i)) / Mt
			//d) Icg = sum(I(i) + covarianMatrix(p(i) - cg) * m(i))
			//e) T0 = sum(w(i) x (I(i) * w(i))
			//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
			//g) Bcg = (Icg ^ -1) * (T0 + T1)

			ndVector com(ndVector::m_zero);
			ndFixSizeArray<ndVector, 8> bodiesCom;
			for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
			{
				const ndBodyDynamic* const body = m_bodies[i];
				const ndMatrix matrix (body->GetMatrix());
				ndVector bodyCom(matrix.TransformVector(body->GetCentreOfMass()));
				bodiesCom.PushBack(bodyCom);
				com += bodyCom.Scale (body->GetMassMatrix().m_w);
			}
			m_com = com.Scale(m_invMass);

			ndMatrix inertia(ndGetZeroMatrix());
			ndVector gyroTorque(ndVector::m_zero);
			for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
			{
				const ndBodyDynamic* const body = m_bodies[i];

				const ndMatrix matrix(body->GetMatrix());
				const ndVector omega(body->GetOmega());
				const ndVector comDist((bodiesCom[i] - m_com) & ndVector::m_triplexMask);

				m_comDist[i] = comDist;
				ndMatrix covariance(ndCovarianceMatrix(comDist, comDist));
				ndMatrix bodyInertia(body->CalculateInertiaMatrix());
				
				ndFloat32 mass = body->GetMassMatrix().m_w;
				inertia.m_front += (bodyInertia.m_front - covariance.m_front.Scale (mass));
				inertia.m_up += (bodyInertia.m_up - covariance.m_up.Scale(mass));
				inertia.m_right += (bodyInertia.m_right - covariance.m_right.Scale(mass));

				ndFloat32 massDist2 = comDist.DotProduct(comDist).GetScalar() * mass;
				inertia.m_front.m_x += massDist2;
				inertia.m_front.m_y += massDist2;
				inertia.m_front.m_z += massDist2;

				gyroTorque += omega.CrossProduct(bodyInertia.RotateVector(omega));
			}

			m_gyroTorque = gyroTorque;
			inertia.m_posit = ndVector::m_wOne;
			m_invInertia = inertia.Inverse4x4();

			m_hasSupport = false;
			ndBodyKinematic::ndContactMap::Iterator it(m_ballBody->GetContactMap());
			for (it.Begin(); it; it++)
			{
				ndContact* const contact = it.GetNode()->GetInfo();
				if (contact->IsActive())
				{
					world->CalculateJointContacts(contact);
					m_hasSupport = true;
				}
			}
		}

		//e) T0 = sum(w(i) x (I(i) * w(i))
		//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
		//g) Bcg = (Icg ^ -1) * (T0 + T1)
		ndVector CalculateAlpha()
		{
			ndVector torque(m_gyroTorque);

			m_invDynamicsSolver.Solve();
			for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
			{
				ndBodyDynamic* const body = m_bodies[i];
				
				ndVector r(m_comDist[i]);
				ndVector f(m_invDynamicsSolver.GetBodyForce(body));
				ndVector t(m_invDynamicsSolver.GetBodyTorque(body));
				torque += (t + r.CrossProduct(f));
			}
			return m_invInertia.RotateVector(torque);
		}

		void Update(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::Update(world, timestep);
			//ndSkeletonContainer* const skeleton = m_bodies[0]->GetSkeleton();
			//ndAssert(skeleton);
			//m_invDynamicsSolver.SolverBegin(skeleton, nullptr, 0, world, timestep);
			//
			//ndVector alpha(CalculateAlpha());
			//if (ndAbs(alpha.m_z) > ND_ALPHA_TOL)
			//{
			//	//ndTrace(("%d alpha(%f) angle(%f)\n", xxx, alpha.m_z, m_controlJoint->GetOffsetAngle() * ndRadToDegree));
			//}
			//m_invDynamicsSolver.SolverEnd();
		}

		void PostUpdate(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::PostUpdate(world, timestep);

			InitState(world);
			if (m_hasSupport)
			//if (0)
			{
				ndSkeletonContainer* const skeleton = m_bodies[0]->GetSkeleton();
				ndAssert(skeleton);
			
				m_invDynamicsSolver.SolverBegin(skeleton, nullptr, 0, world, timestep);
				ndVector alpha(CalculateAlpha());
				if (ndAbs(alpha.m_z) > ND_ALPHA_TOL)
				{
					//ndFloat32 angle = m_controlJoint->GetOffsetAngle();
					ndFloat32 angle = m_controlJoint->GetAngle();
					//ndTrace(("%d alpha(%f) angle(%f)  deltaAngle(%f)\n", xxx, alpha.m_z, angle * ndRadToDegree, 0.0f));
			
					ndInt32 passes = 1;
					ndFloat32 angleLimit = ndFloat32(45.0f * ndDegreeToRad);
					do
					{
						passes--;
						ndFloat32 deltaAngle = alpha.m_z * 0.002f;
						angle += deltaAngle;
						
						angle = ndClamp(angle + deltaAngle, -angleLimit, angleLimit);
						m_controlJoint->SetTargetAngle(angle);
						m_invDynamicsSolver.UpdateJointAcceleration(m_controlJoint);
						alpha = CalculateAlpha();
						//ndTrace(("%d alpha(%f) angle(%f)  deltaAngle(%f)\n", xxx, alpha.m_z, angle * ndRadToDegree, deltaAngle));
					} while ((ndAbs(alpha.m_z) > ND_ALPHA_TOL) && passes);
					//ndTrace(("\n"));
				}
				m_crossValidation____ = CalculateAlpha();
			
				ndFloat32 xxx0 = m_controlJoint->GetTargetAngle() - m_controlJoint->GetAngle();
				ndTrace(("deltaAngle(%f)\n", xxx0 * ndRadToDegree));
				m_invDynamicsSolver.SolverEnd();
			}
		}
#else
		void Update(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::Update(world, timestep);
		}

		void PostUpdate(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::PostUpdate(world, timestep);
			if (ValidateContact(world))
			{
				ndAssert(m_bodies[0]->GetSkeleton());
				ndSkeletonContainer* const skeleton = m_bodies[0]->GetSkeleton();

				m_invDynamicsSolver.SolverBegin(skeleton, nullptr, 0, world, timestep);

				//a) Mt = sum(m(i))
				//b) cg = sum(p(i) * m(i)) / Mt
				//c) Vcg = sum(v(i) * m(i)) / Mt
				//d) Icg = sum(I(i) + covarianMatrix(p(i) - cg) * m(i))
				//e) T0 = sum[w(i) x (I(i) * w(i)) - Vcg x (m(i) * v(i))]
				//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
				//g) Bcg = (Icg ^ -1) * (T0 + T1)

				//a) Mt = sum(m(i))
				//b) cg = sum(p(i) * m(i)) / Mt
				//f) T = sum[(p(i) - cg) x Fext(i) + Text(i) + w(i) x (I(i) * w(i)]

				m_invDynamicsSolver.Solve();
				ndVector com(ndVector::m_zero);
				ndFixSizeArray<ndVector, 8> bodiesCom;
				for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
				{
					const ndBodyDynamic* const body = m_bodies[i];
					const ndMatrix matrix(body->GetMatrix());
					ndVector bodyCom(matrix.TransformVector(body->GetCentreOfMass()));
					bodiesCom.PushBack(bodyCom);
					com += bodyCom.Scale(body->GetMassMatrix().m_w);
				}
				com = com.Scale(m_invMass);

				ndVector torque(ndVector::m_zero);
				for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
				{
					const ndBodyDynamic* const body = m_bodies[i];
					const ndVector omega(body->GetOmega());
					const ndMatrix bodyInertia(body->CalculateInertiaMatrix());
					const ndVector force(m_invDynamicsSolver.GetBodyForce(body));
					const ndVector comDist((bodiesCom[i] - com) & ndVector::m_triplexMask);

					torque += m_invDynamicsSolver.GetBodyTorque(body);
					torque += comDist.CrossProduct(force);
					torque += omega.CrossProduct(bodyInertia.RotateVector(omega));
				}

				m_invDynamicsSolver.SolverEnd();
			}
		}
#endif

		//ndMatrix m_invInertia;
		//ndVector m_com;
		//ndVector m_comVel;
		//ndVector m_gyroTorque;

		ndFixSizeArray<ndVector, 8> m_comDist;
		ndFixSizeArray<ndBodyDynamic*, 8> m_bodies;
		ndBodyDynamic* m_ballBody;
		ndJointHinge* m_controlJoint;
		ndFloat32 m_invMass;
		ndReal m_actionMap[ND_TOTAL_ACTIONS];
		//bool m_hasSupport;
		//ndVector m_crossValidation____;
	};

	// implements a DQN to keep the robot balanced
	class ndModelUnicycleTrainer : public ndModelUnicycle
	{
		public:
		enum class ndTrainingStage
		{
			m_starTraining,
			m_populateReplay,
			//m_initExploration,
			//m_exploration,
			//m_exploitExplore,
			//m_initTraining,
			//m_tickTrainingEpoch,
			//m_endTraining,
		};

		class ndBasePose
		{
			public:
			ndBasePose()
				:m_body(nullptr)
			{
			}

			ndBasePose(ndBodyDynamic* const body)
				:m_veloc(body->GetVelocity())
				,m_omega(body->GetOmega())
				,m_posit(body->GetPosition())
				,m_rotation(body->GetRotation())
				,m_body(body)
			{
			}

			void SetPose()
			{
				m_body->SetMatrix(ndCalculateMatrix(m_rotation, m_posit));
				m_body->SetOmega(m_omega);
				m_body->SetVelocity(m_veloc);
			}

			ndVector m_veloc;
			ndVector m_omega;
			ndVector m_posit;
			ndQuaternion m_rotation;
			ndBodyDynamic* m_body;
		};

		ndModelUnicycleTrainer()
			:ndModelUnicycle()
			,m_currentTransition()
			,m_replayBuffer()
			,m_basePose()
			,m_traingCounter(0)
			,m_trainingState(ndTrainingStage::m_starTraining)
			,m_epsilonGreedy(ndFloat32 (1.0f))
		{
			m_replayBuffer.SetSize(1024 * 128);
			// memories size 100000, batch size 256
			ndSetRandSeed(42);
		}

		void InitState()
		{
			ndAssert(0);
#if 0
			//a) Mt = sum(m(i))
			//b) cg = sum(p(i) * m(i)) / Mt
			//c) Vcg = sum(v(i) * m(i)) / Mt
			//d) Icg = sum(I(i) + covarianMatrix(p(i) - cg) * m(i))
			//e) T0 = sum[w(i) x (I(i) * w(i)) - Vcg x (m(i) * v(i))]
			//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
			//g) Bcg = (Icg ^ -1) * (T0 + T1)

			//a) Mt = sum(m(i))
			//b) cg = sum(p(i) * m(i)) / Mt
			//d) Icg = sum(I(i) + covarianMatrix(p(i) - cg) * m(i))
			//e) T0 = sum(w(i) x (I(i) * w(i))
			//f) T1 = sum[(p(i) - cg) x Fext(i) + Text(i)]
			//g) Bcg = (Icg ^ -1) * (T0 + T1)

			ndVector com(ndVector::m_zero);
			ndFixSizeArray<ndVector, 8> bodiesCom;
			for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
			{
				const ndBodyDynamic* const body = m_bodies[i];
				const ndMatrix matrix(body->GetMatrix());
				ndVector bodyCom(matrix.TransformVector(body->GetCentreOfMass()));
				bodiesCom.PushBack(bodyCom);
				com += bodyCom.Scale(body->GetMassMatrix().m_w);
			}
			com = com.Scale(m_invMass);

			ndMatrix inertia(ndGetZeroMatrix());
			ndVector gyroTorque(ndVector::m_zero);
			for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
			{
				const ndBodyDynamic* const body = m_bodies[i];

				const ndMatrix matrix(body->GetMatrix());
				const ndVector omega(body->GetOmega());
				const ndVector comDist((bodiesCom[i] - m_com) & ndVector::m_triplexMask);

				m_comDist[i] = comDist;
				ndMatrix bodyInertia(body->CalculateInertiaMatrix());
				ndMatrix covariance(ndCovarianceMatrix(comDist, comDist));

				ndFloat32 mass = body->GetMassMatrix().m_w;
				inertia.m_front += (bodyInertia.m_front - covariance.m_front.Scale(mass));
				inertia.m_up += (bodyInertia.m_up - covariance.m_up.Scale(mass));
				inertia.m_right += (bodyInertia.m_right - covariance.m_right.Scale(mass));

				ndFloat32 massDist2 = comDist.DotProduct(comDist).GetScalar();
				inertia.m_front.m_x += massDist2;
				inertia.m_front.m_y += massDist2;
				inertia.m_front.m_z += massDist2;

				gyroTorque += omega.CrossProduct(bodyInertia.RotateVector(omega));
			}

			m_gyroTorque = gyroTorque;
			inertia.m_posit = ndVector::m_wOne;
			m_invInertia = inertia.Inverse4x4();
#endif
		}

		void StartEpisode()
		{
			for (ndInt32 i = 0; i < m_basePose.GetCount(); i++)
			{
				m_basePose[i].SetPose();
			}
			m_currentTransition.Clear();

			//m_currentTransition.m_action = 0;
			SelectAction();
		}

		void TrainingLoopBegin(ndWorld* const, ndFloat32)
		{
			switch (m_trainingState)
			{
				case ndTrainingStage::m_starTraining:
				{
					StartEpisode();
					m_replayBuffer.SetCount(0);
					m_trainingState = ndTrainingStage::m_populateReplay;
					break;
				}

				case ndTrainingStage::m_populateReplay:
				{
					if (m_currentTransition.m_terminalState)
					{
						StartEpisode();
					}

					if (m_replayBuffer.GetCount() == m_replayBuffer.GetCapacity())
					{
						ndAssert(0);
					}

					break;
				}
				
				default:;
					ndAssert(0);
			}
		}

		void SelectAction()
		{
			ndFloat32 explore = ndRand();
			if (explore <= m_epsilonGreedy)
			{
				// explore actions;
				ndInt32 action = ndInt32(ndRandInt() % ND_TOTAL_ACTIONS);
				m_currentTransition.m_action[0] = action;
			}
			else
			{
				// exploit actions
				ndAssert(0);
			}
		}

		ndFloat32 GetStateReward()
		{
			const ndBodyKinematic* const body = GetRoot()->m_body->GetAsBodyKinematic();
			ndFloat32 value = body->GetMatrix().m_up.m_y;
			return value;
		}

		void Update(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::Update(world, timestep);
			 
			// apply the action
			ndFloat32 action = m_actionMap[m_currentTransition.m_action[0]];
			ndFloat32 angle = ndClamp(m_controlJoint->GetAngle() + action, ndFloat32(-45.0f) * ndDegreeToRad, ndFloat32(45.0f) * ndDegreeToRad);
			m_controlJoint->SetTargetAngle(angle);
		}

		void PostUpdate(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::PostUpdate(world, timestep);

			//if (ValidateContact(world))
			{
				//Get state t + 1, from model and stored on replay buffer.
				ndAssert(m_bodies[0]->GetSkeleton());
				ndSkeletonContainer* const skeleton = m_bodies[0]->GetSkeleton();

				//a) Mt = sum(m(i))
				//b) cg = sum(p(i) * m(i)) / Mt
				//f) T = sum[(p(i) - cg) x Fext(i) + Text(i) + w(i) x (I(i) * w(i)]

				ValidateContact(world);
				m_invDynamicsSolver.SolverBegin(skeleton, nullptr, 0, world, timestep);
				m_invDynamicsSolver.Solve();

				ndVector com(ndVector::m_zero);
				ndFixSizeArray<ndVector, 8> bodiesCom;
				for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
				{
					const ndBodyDynamic* const body = m_bodies[i];
					const ndMatrix matrix(body->GetMatrix());
					ndVector bodyCom(matrix.TransformVector(body->GetCentreOfMass()));
					bodiesCom.PushBack(bodyCom);
					com += bodyCom.Scale(body->GetMassMatrix().m_w);
				}
				com = com.Scale(m_invMass);

				ndVector torque(ndVector::m_zero);
				for (ndInt32 i = 0; i < m_bodies.GetCount(); ++i)
				{
					const ndBodyDynamic* const body = m_bodies[i];
					const ndVector omega(body->GetOmega());
					const ndMatrix bodyInertia(body->CalculateInertiaMatrix());
					const ndVector force(m_invDynamicsSolver.GetBodyForce(body));
					const ndVector comDist((bodiesCom[i] - com) & ndVector::m_triplexMask);

					torque += m_invDynamicsSolver.GetBodyTorque(body);
					torque += comDist.CrossProduct(force);
					torque += omega.CrossProduct(bodyInertia.RotateVector(omega));
				}
				m_invDynamicsSolver.SolverEnd();

				//calculate reward for current state;
				m_currentTransition.m_reward = ndReal(GetStateReward());

				// episode done, whne the upright orienta tion is less than 20 degree 
				if (m_currentTransition.m_reward < ndFloat32(0.95f))
				{
					m_currentTransition.m_reward = ndFloat32(0.0f);
					m_currentTransition.m_terminalState = true;
				}

				m_currentTransition.m_nextState[0] = ndReal(torque.m_z);
				m_currentTransition.m_nextState[1] = ndReal(m_controlJoint->GetAngle());
				m_replayBuffer.AddTransition(m_currentTransition);

				// save the next state 
				m_currentTransition.m_state = m_currentTransition.m_nextState;

				// select the action using epsilon greedy method
				SelectAction();

				TrainingLoopBegin(world, timestep);
			}

			//TrainingLoopEnd(world, timestep);
		}

		ndBrainReplayTransitionMemory<ndInt32, 2, 1> m_currentTransition;
		ndBrainReplayBuffer<ndInt32, 2, 1> m_replayBuffer;
		ndFixSizeArray<ndBasePose, 32> m_basePose;
		ndInt32 m_traingCounter;
		ndTrainingStage m_trainingState;
		ndFloat32 m_epsilonGreedy;
	};

	void BuildModel(ndModelUnicycle* const model, ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		ndFloat32 mass = 10.0f;
		ndFloat32 limbMass = 1.0f;
		ndFloat32 wheelMass = 1.0f;

		ndFloat32 xSize = 0.25f;
		ndFloat32 ySize = 0.40f;
		ndFloat32 zSize = 0.30f;
		ndPhysicsWorld* const world = scene->GetWorld();
		
		// add hip body
		ndSharedPtr<ndBody> hipBody(world->GetBody(AddBox(scene, location, mass, xSize, ySize, zSize, "smilli.tga")));
		ndModelArticulation::ndNode* const modelRoot = model->AddRootBody(hipBody);

		ndMatrix matrix(hipBody->GetMatrix());
		matrix.m_posit.m_y += 0.5f;
		hipBody->SetMatrix(matrix);

		ndMatrix limbLocation(matrix);
		limbLocation.m_posit.m_z += zSize * 0.0f;
		limbLocation.m_posit.m_y -= ySize * 0.5f;
		limbLocation.m_posit.m_x += xSize * 0.5f * -0.1f;

		// make single leg
		ndFloat32 limbLength = 0.3f;
		ndFloat32 limbRadio = 0.025f;

		ndSharedPtr<ndBody> legBody(world->GetBody(AddCapsule(scene, ndGetIdentityMatrix(), limbMass, limbRadio, limbRadio, limbLength, "smilli.tga")));
		ndMatrix legLocation(ndRollMatrix(-90.0f * ndDegreeToRad) * limbLocation);
		legLocation.m_posit.m_y -= limbLength * 0.5f;
		legBody->SetMatrix(legLocation);
		ndMatrix legPivot(ndYawMatrix(90.0f * ndDegreeToRad) * legLocation);
		legPivot.m_posit.m_y += limbLength * 0.5f;
		ndSharedPtr<ndJointBilateralConstraint> legJoint(new ndJointHinge(legPivot, legBody->GetAsBodyKinematic(), modelRoot->m_body->GetAsBodyKinematic()));
		ndJointHinge* const hinge = (ndJointHinge*)*legJoint;
		hinge->SetAsSpringDamper(0.001f, 1500, 40.0f);
		model->m_controlJoint = hinge;

		// make wheel
		ndFloat32 wheelRadio = 4.0f * limbRadio;
		ndSharedPtr<ndBody> wheelBody(world->GetBody(AddSphere(scene, ndGetIdentityMatrix(), wheelMass, wheelRadio, "smilli.tga")));
		ndMatrix wheelMatrix(legPivot);
		wheelMatrix.m_posit.m_y -= limbLength;
		wheelBody->SetMatrix(wheelMatrix);
		ndSharedPtr<ndJointBilateralConstraint> wheelJoint(new ndJointSpherical(wheelMatrix, wheelBody->GetAsBodyKinematic(), legBody->GetAsBodyKinematic()));
		//((ndJointSpherical*)*wheelJoint)->SetAsSpringDamper(ndFloat32(0.001f), ndFloat32(0.0f), ndFloat32(10.f));

		// tele port the model so that is on the floor
		ndMatrix probeMatrix(wheelMatrix);
		probeMatrix.m_posit.m_x += 1.0f;
		ndMatrix floor(FindFloor(*world, probeMatrix, wheelBody->GetAsBodyKinematic()->GetCollisionShape(), 20.0f));
		ndFloat32 dist = wheelMatrix.m_posit.m_y - floor.m_posit.m_y;

		ndMatrix rootMatrix(modelRoot->m_body->GetMatrix());

		rootMatrix.m_posit.m_y -= dist;
		wheelMatrix.m_posit.m_y -= dist;
		legLocation.m_posit.m_y -= dist;

		legBody->SetMatrix(legLocation);
		wheelBody->SetMatrix(wheelMatrix);
		modelRoot->m_body->SetMatrix(rootMatrix);

		legBody->GetNotifyCallback()->OnTransform(0, legLocation);
		wheelBody->GetNotifyCallback()->OnTransform(0, wheelMatrix);
		modelRoot->m_body->GetNotifyCallback()->OnTransform(0, rootMatrix);

		// add the joints manually, because on this model the wheel is not actuated.
		world->AddJoint(legJoint);
		world->AddJoint(wheelJoint);

		// add model limbs
		model->AddLimb(modelRoot, legBody, legJoint);

		model->m_ballBody = wheelBody->GetAsBodyDynamic();

		model->Init();
	}

	ndModelArticulation* CreateModel(ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		ndModelUnicycle* const model = new ndModelUnicycle();
		BuildModel(model, scene, location);

		return model;
	}

	ndModelArticulation* CreateTrainer(ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		ndModelUnicycleTrainer* const model = new ndModelUnicycleTrainer();
		BuildModel(model, scene, location);

		//for (ndModelArticulation::ndNode* node = model->GetRoot()->GetFirstIterator(); node; node = node->GetNextIterator())
		for (ndInt32 i = 0; i < model->m_bodies.GetCount(); ++i)
		{
			model->m_basePose.PushBack(model->m_bodies[i]);
		}
		model->m_basePose.PushBack(model->m_ballBody);

		scene->SetAcceleratedUpdate();
		return model;
	}
}

using namespace ndController_1;
void ndBalanceController(ndDemoEntityManager* const scene)
{
	// build a floor
	//BuildFloorBox(scene, ndGetIdentityMatrix());
	BuildFlatPlane(scene, true);
	
	ndWorld* const world = scene->GetWorld();
	ndMatrix matrix(ndYawMatrix(-0.0f * ndDegreeToRad));

	//ndSharedPtr<ndModel> model(CreateModel(scene, matrix));
	ndSharedPtr<ndModel> model(CreateTrainer(scene, matrix));
	scene->GetWorld()->AddModel(model);

	ndModelArticulation* const articulation = (ndModelArticulation*)model->GetAsModelArticulation();
	ndBodyKinematic* const rootBody = articulation->GetRoot()->m_body->GetAsBodyKinematic();
	ndSharedPtr<ndJointBilateralConstraint> fixJoint(new ndJointPlane(rootBody->GetMatrix().m_posit, ndVector(0.0f, 0.0f, 1.0f, 0.0f), rootBody, world->GetSentinelBody()));
	world->AddJoint(fixJoint);
	
	matrix.m_posit.m_x -= 0.0f;
	matrix.m_posit.m_y += 0.5f;
	matrix.m_posit.m_z += 2.0f;
	ndQuaternion rotation(ndVector(0.0f, 1.0f, 0.0f, 0.0f), 90.0f * ndDegreeToRad);
	scene->SetCameraMatrix(rotation, matrix.m_posit);
}
