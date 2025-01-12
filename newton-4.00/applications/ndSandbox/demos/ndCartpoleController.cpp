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

//#define D_USE_POLE_DQN

namespace ndController_0
{
	#define D_PUSH_ACCEL			ndFloat32 (15.0f)
	#define D_REWARD_MIN_ANGLE		(ndFloat32 (20.0f) * ndDegreeToRad)

#ifdef D_USE_POLE_DQN
	enum ndActionSpace
	{
		m_doNoting,
		m_pushLeft,
		m_pushRight,
		m_actionsSize
	};
#else
	enum ndActionSpace
	{
		m_doNoting,
		m_actionsSize
	};
#endif

	enum ndStateSpace
	{
		m_poleAngle,
		m_poleOmega,
		m_stateSize
	};

	class ndCartpole: public ndModelArticulation
	{
		public:
		#ifdef D_USE_POLE_DQN
			class ndCartpoleAgent : public ndBrainAgentDQN<m_stateSize, m_actionsSize>
			{
				public:
				ndCartpoleAgent(ndSharedPtr<ndBrain>& actor)
					:ndBrainAgentDQN<m_stateSize, m_actionsSize>(actor)
					,m_model(nullptr)
				{
				}

				void GetObservation(ndReal* const state) const
				{
					m_model->GetObservation(state);
				}

				virtual void ApplyActions(ndReal* const actions) const
				{
					m_model->ApplyActions(actions);
				}

				ndCartpole* m_model;
			};

			class ndCartpoleAgent_trainer : public ndBrainAgentDQN_Trainer<m_stateSize, m_actionsSize>
			{
				public:
				ndCartpoleAgent_trainer(ndSharedPtr<ndBrain>& qValuePredictor)
					:ndBrainAgentDQN_Trainer<m_stateSize, m_actionsSize>(qValuePredictor)
					,m_model(nullptr)
					,m_stopTraining(3000000)
					,m_maxGain(-1.0e10f)
					,m_maxFrames(1200.0f)
					,m_controllerState(0)
					,m_averageQValue()
					,m_averageFramesPerEpisodes()
				{
					m_outFile = fopen("traingPerf.csv", "wb");
				}

				~ndCartpoleAgent_trainer()
				{
					if (m_outFile)
					{
						fclose(m_outFile);
					}
				}

				ndReal GetReward() const
				{
					return m_model->GetReward();
				}

				virtual void ApplyActions(ndReal* const actions) const
				{
					if (GetEpisodeFrames() >= 2500)
					{
						ndUnsigned32 randomIndex = ndRandInt();
						*actions = ndReal (ndInt32(randomIndex % 3));
					}

					m_model->ApplyActions(actions);
				}

				void GetObservation(ndReal* const state) const
				{
					m_model->GetObservation(state);
				}

				bool IsTerminal() const
				{
					bool state = m_model->IsTerminal();
					if (!IsSampling())
					{
						m_averageQValue.Update(GetCurrentValue());
						if (state)
						{
							m_averageFramesPerEpisodes.Update(ndReal(GetEpisodeFrames()));
						}
					}

					return state;
				}

				void ResetModel() const
				{
					m_controllerState = -1;
					m_model->ResetModel();
				}

				void Step()
				{
					switch (m_controllerState)
					{
						case 0:
						{
							m_model->RandomePush();
							break;
						}
						case 1:
						{
							break;
						}
						default:
						{
							ndBrainAgentDQN_Trainer::Step();
						}
					}
				}

				void OptimizeStep()
				{
					switch (m_controllerState)
					{
						case 0:
						case 1:
							break;

						default:
						{
							ndInt32 stopTraining = GetFramesCount();
							if (stopTraining <= m_stopTraining)
							{
								ndInt32 episodeCount = GetEposideCount();
								ndBrainAgentDQN_Trainer::OptimizeStep();
								episodeCount -= GetEposideCount();

								if (m_averageFramesPerEpisodes.GetAverage() >= m_maxFrames)
								{
									if (episodeCount && (m_averageQValue.GetAverage() > m_maxGain))
									{
										char fileName[1024];
										ndGetWorkingFileName(GetName().GetStr(), fileName);

										SaveToFile(fileName);
										ndExpandTraceMessage("saving to file: %s\n", fileName);
										ndExpandTraceMessage("episode: %d\taverageFrames: %f\taverageValue %f\n\n", GetEposideCount(), m_averageFramesPerEpisodes.GetAverage(), m_averageQValue.GetAverage());
										m_maxGain = m_averageQValue.GetAverage();
									}
								}

								if (episodeCount && !IsSampling())
								{
									ndExpandTraceMessage("%g\n", m_averageQValue.GetAverage());
									if (m_outFile)
									{
										fprintf(m_outFile, "%g\n", m_averageQValue.GetAverage());
										fflush(m_outFile);
									}
								}

								if (stopTraining == m_stopTraining)
								{
									ndExpandTraceMessage("\n");
									ndExpandTraceMessage("training complete\n");
								}
							}
							
							if (m_model->IsOutOfBounds())
							{
								m_model->TelePort();
							}
						}
					}

					m_controllerState++;
				}

				FILE* m_outFile;
				ndCartpole* m_model;
				ndInt32 m_stopTraining;
				ndFloat32 m_maxGain;
				ndFloat32 m_maxFrames;
				mutable ndInt32 m_controllerState;
				mutable ndMovingAverage<256> m_averageQValue;
				mutable ndMovingAverage<128> m_averageFramesPerEpisodes;
			};

		#else

			class ndCartpoleAgent: public ndBrainAgentDDPG<m_stateSize, m_actionsSize>
			{
				public:
				ndCartpoleAgent(ndSharedPtr<ndBrain>& actor)
					:ndBrainAgentDDPG<m_stateSize, m_actionsSize>(actor)
					,m_model(nullptr)
				{
				}

				void GetObservation(ndReal* const state) const
				{
					m_model->GetObservation(state);
				}

				virtual void ApplyActions(ndReal* const actions) const
				{
					m_model->ApplyActions(actions);
				}

				ndCartpole* m_model;
			};

			class ndCartpoleAgent_trainer : public ndBrainAgentDDPG_Trainer<m_stateSize, m_actionsSize>
			{
				public:
				ndCartpoleAgent_trainer(ndSharedPtr<ndBrain>& actor, ndSharedPtr<ndBrain>& critic)
					:ndBrainAgentDDPG_Trainer<m_stateSize, m_actionsSize>(actor, critic)
					,m_model(nullptr)
					,m_stopTraining(1000000)
					,m_maxGain(-1.0e10f)
					,m_maxFrames(200.0f)
					,m_controllerState(0)
					,m_averageQValue()
					,m_averageFramesPerEpisodes()
				{
					m_outFile = fopen("traingPerf.csv", "wb");
				}

				~ndCartpoleAgent_trainer()
				{
					if (m_outFile)
					{
						fclose(m_outFile);
					}
				}

				ndReal GetReward() const
				{
					return m_model->GetReward();
				}

				virtual void ApplyActions(ndReal* const actions) const
				{
					if (GetEpisodeFrames() >= 10000)
					{
						for (ndInt32 i = 0; i < m_actionsSize; ++i)
						{
							ndReal veryNoisyAction = ndReal(ndClamp(100.0f * ndReal (-actions[i]), ndFloat32(-1.0f), ndFloat32(1.0f)));
							actions[i] = veryNoisyAction;
						}
					}
					else if (GetEpisodeFrames() >= 2500)
					{
						for (ndInt32 i = 0; i < m_actionsSize; ++i)
						{
							ndReal veryNoisyAction = ndReal(ndClamp(ndGaussianRandom(ndFloat32(actions[i]), ndFloat32(0.25f)), ndFloat32(-1.0f), ndFloat32(1.0f)));
							actions[i] = veryNoisyAction;
						}
					}
					//ndAssert(((m_controllerState == 2) && (actions[0] >= 0.0f)) || (m_controllerState != 2));
					m_model->ApplyActions(actions);
				}

				void GetObservation(ndReal* const state) const
				{
					m_model->GetObservation(state);
				}

				bool IsTerminal() const
				{
					bool state = m_model->IsTerminal();
					if (!IsSampling())
					{
						m_averageQValue.Update(GetCurrentValue());
						if (state)
						{
							m_averageFramesPerEpisodes.Update(ndReal(GetEpisodeFrames()));
						}
					}
					return state;
				}

				void ResetModel() const
				{
					m_controllerState = -1;
					m_model->ResetModel();
				}

				void Step()
				{
					switch (m_controllerState)
					{
						case 0:
						{
							//m_model->RandomePush();
							break;
						}
						case 1:
						{
							break;
						}
						default:
						{
							ndBrainAgentDDPG_Trainer::Step();
						}
					}
				}

				void OptimizeStep()
				{
					switch (m_controllerState)
					{
						case 0:
							break;
						case 1:
							break;

						default:
						{
							ndInt32 stopTraining = GetFramesCount();
							if (stopTraining <= m_stopTraining)
							{
								ndInt32 episodeCount = GetEposideCount();
								ndBrainAgentDDPG_Trainer::OptimizeStep();

								episodeCount -= GetEposideCount();
								if (m_averageFramesPerEpisodes.GetAverage() >= m_maxFrames)
								{
									if (episodeCount && (m_averageQValue.GetAverage() > m_maxGain))
									{
										char fileName[1024];
										ndGetWorkingFileName(GetName().GetStr(), fileName);

										SaveToFile(fileName);
										ndExpandTraceMessage("saving to file: %s\n", fileName);
										ndExpandTraceMessage("episode: %d\taverageFrames: %f\taverageValue %f\n\n", GetEposideCount(), m_averageFramesPerEpisodes.GetAverage(), m_averageQValue.GetAverage());
										m_maxGain = m_averageQValue.GetAverage();
									}
								}

								if (episodeCount && !IsSampling())
								{
									//ndExpandTraceMessage("%g\n", m_averageQValue.GetAverage());
									if (m_outFile)
									{
										fprintf(m_outFile, "%g\n", m_averageQValue.GetAverage());
										fflush(m_outFile);
									}
								}

								if (stopTraining == m_stopTraining)
								{
									ndExpandTraceMessage("\n");
									ndExpandTraceMessage("training complete\n");
								}
							}

							if (m_model->IsOutOfBounds())
							{
								m_model->TelePort();
							}
						}
					}

					m_controllerState++;
				}

				FILE* m_outFile;
				ndCartpole* m_model;
				ndInt32 m_stopTraining;
				ndFloat32 m_maxGain;
				ndFloat32 m_maxFrames;
				mutable ndInt32 m_controllerState;
				mutable ndMovingAverage<256> m_averageQValue;
				mutable ndMovingAverage<128> m_averageFramesPerEpisodes;
			};

		#endif

		ndCartpole(const ndSharedPtr<ndBrainAgent>& agent)
			:ndModelArticulation()
			,m_cartMatrix(ndGetIdentityMatrix())
			,m_poleMatrix(ndGetIdentityMatrix())
			,m_cart(nullptr)
			,m_pole(nullptr)
			,m_agent(agent)
		{
		}

		bool IsTerminal() const
		{
			const ndMatrix& matrix = m_pole->GetMatrix();
			// agent dies if the angle is larger than D_REWARD_MIN_ANGLE * ndFloat32 (2.0f) degrees
			bool fail = ndAbs(ndAsin (matrix.m_front.m_x)) > (D_REWARD_MIN_ANGLE * ndFloat32 (2.0f));
			return fail;
		}

		ndReal GetReward() const
		{
			const ndMatrix& matrix = m_pole->GetMatrix();
			ndFloat32 sinAngle = matrix.m_front.m_x;
			ndFloat32 reward = ndReal(ndPow(ndEXP, - ndFloat32 (100.0f) * sinAngle * sinAngle));
			return ndReal(reward);
		}

		void ApplyActions(ndReal* const actions) const
		{
			ndVector force(m_cart->GetForce());
			#ifdef D_USE_POLE_DQN
				ndInt32 action = ndInt32(actions[0]);
				if (action == m_pushLeft)
				{
					force.m_x = -m_cart->GetMassMatrix().m_w * D_PUSH_ACCEL;
				}
				else if (action == m_pushRight)
				{
					force.m_x = m_cart->GetMassMatrix().m_w * D_PUSH_ACCEL;
				}
			#else
				ndFloat32 action = actions[0];
				force.m_x = 2.0f * action * (m_cart->GetMassMatrix().m_w * D_PUSH_ACCEL);
			#endif
			m_cart->SetForce(force);
		}

		void GetObservation(ndReal* const state)
		{
			ndVector omega(m_pole->GetOmega());
			const ndMatrix& matrix = m_pole->GetMatrix();
			ndFloat32 angle = ndAsin (matrix.m_front.m_x);
			state[m_poleAngle] = ndReal(angle);
			state[m_poleOmega] = ndReal(omega.m_z);
		}

		void TelePort() const
		{
			ndVector veloc(m_cart->GetVelocity());
			ndVector posit(m_cart->GetMatrix().m_posit & ndVector::m_triplexMask);
			posit.m_y = 0.0f;
			posit.m_z = 0.0f;
			veloc.m_y = 0.0f;
			veloc.m_z = 0.0f;

			ndMatrix cartMatrix(m_cart->GetMatrix());
			ndVector cartVeloc(m_cart->GetVelocity());
			cartVeloc -= veloc;
			cartMatrix.m_posit -= posit;
			m_cart->SetMatrix(cartMatrix);
			m_cart->SetVelocity(cartVeloc);

			ndMatrix poleMatrix(m_pole->GetMatrix());
			ndVector poleVeloc(m_pole->GetVelocity());
			poleVeloc -= veloc;
			poleMatrix.m_posit -= posit;
			m_pole->SetMatrix(poleMatrix);
			m_pole->SetVelocity(poleVeloc);
		}

		void ResetModel() const
		{
			m_cart->SetMatrix(m_cartMatrix);
			m_pole->SetMatrix(m_poleMatrix);

			m_pole->SetOmega(ndVector::m_zero);
			m_pole->SetVelocity(ndVector::m_zero);

			m_cart->SetOmega(ndVector::m_zero);
			m_cart->SetVelocity(ndVector::m_zero);
		}

		void RandomePush()
		{
			ndVector impulsePush(ndVector::m_zero);
			impulsePush.m_x = 5.0f * m_cart->GetMassMatrix().m_w *  ndSquash (ndGaussianRandom(0.0f, 0.5f));
			m_cart->ApplyImpulsePair(impulsePush, ndVector::m_zero, m_cart->GetScene()->GetTimestep());
		}

		bool IsOutOfBounds() const
		{
			return ndAbs(m_cart->GetMatrix().m_posit.m_x) > ndFloat32(20.0f);
		}

		void Update(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::Update(world, timestep);
			m_agent->Step();
		}

		void PostUpdate(ndWorld* const world, ndFloat32 timestep)
		{
			ndModelArticulation::PostUpdate(world, timestep);
			m_agent->OptimizeStep();
		}

		ndMatrix m_cartMatrix;
		ndMatrix m_poleMatrix;
		ndBodyDynamic* m_cart;
		ndBodyDynamic* m_pole;
		ndSharedPtr<ndBrainAgent> m_agent;
	};

	void BuildModel(ndCartpole* const model, ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		ndFloat32 xSize = 0.25f;
		ndFloat32 ySize = 0.125f;
		ndFloat32 zSize = 0.15f;
		ndFloat32 cartMass = 5.0f;
		ndFloat32 poleMass = 10.0f;
		ndFloat32 poleLength = 0.4f;
		ndFloat32 poleRadio = 0.05f;
		ndPhysicsWorld* const world = scene->GetWorld();
		
		// make cart
		ndSharedPtr<ndBody> cartBody(world->GetBody(AddBox(scene, location, cartMass, xSize, ySize, zSize, "smilli.tga")));
		ndModelArticulation::ndNode* const modelRoot = model->AddRootBody(cartBody);
		ndMatrix matrix(cartBody->GetMatrix());
		matrix.m_posit.m_y += ySize / 2.0f;
		cartBody->SetMatrix(matrix);
		cartBody->GetAsBodyDynamic()->SetSleepAccel(cartBody->GetAsBodyDynamic()->GetSleepAccel() * ndFloat32(0.1f));
		
		matrix.m_posit.m_y += ySize / 2.0f;

		// make pole leg
		ndSharedPtr<ndBody> poleBody(world->GetBody(AddCapsule(scene, ndGetIdentityMatrix(), poleMass, poleRadio, poleRadio, poleLength, "smilli.tga")));
		ndMatrix poleLocation(ndRollMatrix(90.0f * ndDegreeToRad) * matrix);
		poleLocation.m_posit.m_y += poleLength * 0.5f;
		poleBody->SetMatrix(poleLocation);
		poleBody->GetAsBodyDynamic()->SetSleepAccel(poleBody->GetAsBodyDynamic()->GetSleepAccel() * ndFloat32(0.1f));
		
		// link cart and body with a hinge
		ndMatrix polePivot(ndYawMatrix(90.0f * ndDegreeToRad) * poleLocation);
		polePivot.m_posit.m_y -= poleLength * 0.5f;
		ndSharedPtr<ndJointBilateralConstraint> poleJoint(new ndJointHinge(polePivot, poleBody->GetAsBodyKinematic(), modelRoot->m_body->GetAsBodyKinematic()));

		// make the car move alone the z axis only (2d problem)
		ndSharedPtr<ndJointBilateralConstraint> xDirSlider(new ndJointSlider(cartBody->GetMatrix(), cartBody->GetAsBodyDynamic(), world->GetSentinelBody()));
		world->AddJoint(xDirSlider);

		// add path to the model
		world->AddJoint(poleJoint);
		model->AddLimb(modelRoot, poleBody, poleJoint);

		// save some useful data
		model->m_cart = cartBody->GetAsBodyDynamic();
		model->m_pole = poleBody->GetAsBodyDynamic();
		model->m_cartMatrix = cartBody->GetMatrix();
		model->m_poleMatrix = poleBody->GetMatrix();
	}

	ndModelArticulation* CreateTrainModel(ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		// build neutral net controller
		ndInt32 layerSize = 64;
		#ifdef D_USE_POLE_DQN
			ndSharedPtr<ndBrain> actor(new ndBrain());
			ndBrainLayer* const layer0 = new ndBrainLayer(m_stateSize, layerSize, m_tanh);
			ndBrainLayer* const layer1 = new ndBrainLayer(layer0->GetOuputSize(), layerSize, m_tanh);
			ndBrainLayer* const layer2 = new ndBrainLayer(layer1->GetOuputSize(), layerSize, m_tanh);
			ndBrainLayer* const ouputLayer = new ndBrainLayer(layer2->GetOuputSize(), m_actionsSize, m_lineal);
			actor->BeginAddLayer();
			actor->AddLayer(layer0);
			actor->AddLayer(layer1);
			actor->AddLayer(layer2);
			actor->AddLayer(ouputLayer);
			actor->EndAddLayer(ndReal(0.25f));

			ndSharedPtr<ndBrainAgent> agent(new ndCartpole::ndCartpoleAgent_trainer(actor));
			agent->SetName("cartpoleDQN.nn");

		#else

			ndSharedPtr<ndBrain> actor(new ndBrain());
			ndBrainLayer* const layer0 = new ndBrainLayer(m_stateSize, layerSize, m_tanh);
			ndBrainLayer* const layer1 = new ndBrainLayer(layer0->GetOuputSize(), layerSize, m_tanh);
			ndBrainLayer* const layer2 = new ndBrainLayer(layer1->GetOuputSize(), layerSize, m_tanh);
			ndBrainLayer* const ouputLayer = new ndBrainLayer(layer2->GetOuputSize(), m_actionsSize, m_tanh);
			actor->BeginAddLayer();
			actor->AddLayer(layer0);
			actor->AddLayer(layer1);
			actor->AddLayer(layer2);
			actor->AddLayer(ouputLayer);
			actor->EndAddLayer(ndReal(0.25f));

			// clear actors bias so that the actions start centered at zero
			for (ndInt32 i = 0; i < actor->GetCount(); ++i)
			{
				ndBrainLayer* const actorLayer = (*(*actor))[i];
				ndBrainVector& actorLayerBias = actorLayer->GetBias();
				actorLayerBias.Set(ndReal(0.0f));
			}

			ndSharedPtr<ndBrain> critic(new ndBrain());
			ndBrainLayer* const criticLayer0 = new ndBrainLayer(m_stateSize + m_actionsSize, layerSize * 2, m_tanh);
			ndBrainLayer* const criticLayer1 = new ndBrainLayer(criticLayer0->GetOuputSize(), layerSize * 2, m_tanh);
			ndBrainLayer* const criticLayer2 = new ndBrainLayer(criticLayer1->GetOuputSize(), layerSize * 2, m_tanh);
			ndBrainLayer* const criticOuputLayer = new ndBrainLayer(criticLayer2->GetOuputSize(), 1, m_lineal);
			critic->BeginAddLayer();
			critic->AddLayer(criticLayer0);
			critic->AddLayer(criticLayer1);
			critic->AddLayer(criticLayer2);
			critic->AddLayer(criticOuputLayer);
			critic->EndAddLayer(ndReal(0.25f));

			// add a reinforcement learning controller 
			ndSharedPtr<ndBrainAgent> agent(new ndCartpole::ndCartpoleAgent_trainer(actor, critic));
			agent->SetName("cartpoleDDPG.nn");
		#endif

		char fileName[1024];
		ndGetWorkingFileName(agent->GetName().GetStr(), fileName);
		//agent->SaveToFile(fileName);

		ndCartpole* const model = new ndCartpole(agent);
		ndCartpole::ndCartpoleAgent_trainer* const trainer = (ndCartpole::ndCartpoleAgent_trainer*)*agent;
		trainer->m_model = model;

		BuildModel(model, scene, location);

		scene->SetAcceleratedUpdate();
		return model;
	}

	ndModelArticulation* CreateModel(ndDemoEntityManager* const scene, const ndMatrix& location)
	{
		char fileName[1024];
#ifdef D_USE_POLE_DQN
		ndGetWorkingFileName("cartpoleDQN.nn", fileName);
#else
		ndGetWorkingFileName("cartpoleDDPG.nn", fileName);
#endif
	
		ndSharedPtr<ndBrain> actor(ndBrainLoad::Load(fileName));
		ndSharedPtr<ndBrainAgent> agent(new ndCartpole::ndCartpoleAgent(actor));

		ndCartpole* const model = new ndCartpole(agent);
		((ndCartpole::ndCartpoleAgent*)*agent)->m_model = model;
		
		BuildModel(model, scene, location);
		return model;
	}
}

using namespace ndController_0;

void ndCartpoleControllerPlayer(ndDemoEntityManager* const scene)
{
	BuildFlatPlane(scene, true);

	ndSetRandSeed(42);
	ndWorld* const world = scene->GetWorld();
	ndMatrix matrix(ndYawMatrix(-0.0f * ndDegreeToRad));

	ndSharedPtr<ndModel> model(CreateModel(scene, matrix));
	world->AddModel(model);

	matrix.m_posit.m_x -= 0.0f;
	matrix.m_posit.m_y += 0.5f;
	matrix.m_posit.m_z += 2.0f;
	ndQuaternion rotation(ndVector(0.0f, 1.0f, 0.0f, 0.0f), 90.0f * ndDegreeToRad);
	scene->SetCameraMatrix(rotation, matrix.m_posit);
}

void ndCartpoleControllerTrainer(ndDemoEntityManager* const scene)
{
	// build a floor
	//BuildFloorBox(scene, ndGetIdentityMatrix());
	BuildFlatPlane(scene, true);

	ndSetRandSeed(42);
	ndWorld* const world = scene->GetWorld();
	ndMatrix matrix(ndYawMatrix(-0.0f * ndDegreeToRad));

	ndSharedPtr<ndModel> model(CreateTrainModel(scene, matrix));
	world->AddModel(model);
	
	matrix.m_posit.m_x -= 0.0f;
	matrix.m_posit.m_y += 0.5f;
	matrix.m_posit.m_z += 2.0f;
	ndQuaternion rotation(ndVector(0.0f, 1.0f, 0.0f, 0.0f), 90.0f * ndDegreeToRad);
	scene->SetCameraMatrix(rotation, matrix.m_posit);
}
