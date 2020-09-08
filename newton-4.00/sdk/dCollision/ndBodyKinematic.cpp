/* Copyright (c) <2003-2019> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "ndCollisionStdafx.h"
#include "ndContact.h"
#include "ndShapeNull.h"
#include "ndRayCastNotify.h"
#include "ndBodyKinematic.h"

#define D_MINIMUM_MASS	dFloat32(1.0e-5f)
#define D_INFINITE_MASS	dFloat32(1.0e15f)

class ndDummyCollision: public ndShapeNull
{
	public:
	ndDummyCollision()
		:ndShapeNull()
	{
		m_refCount.fetch_add(1);
	}

	~ndDummyCollision()
	{
		m_refCount.fetch_add(-1);
	}

	static ndShapeNull* GetNullShape()
	{
		static ndDummyCollision nullShape;
		return &nullShape;
	}
};

class ndBodyKinematic::ndContactkey
{
	public:
	ndContactkey(dUnsigned32 tag0, dUnsigned32 tag1)
		:m_tagLow(dMin(tag0, tag1))
		,m_tagHigh(dMax(tag0, tag1))
	{
		dAssert(m_tagLow < m_tagHigh);
	}

	bool operator== (const ndContactkey& key) const
	{
		return m_tag == key.m_tag;
	}

	bool operator< (const ndContactkey& key) const
	{
		return m_tag < key.m_tag;
	}

	bool operator> (const ndContactkey& key) const
	{
		return m_tag > key.m_tag;
	}

	private:
	union
	{
		dUnsigned64 m_tag;
		struct
		{
			dUnsigned32 m_tagLow;
			dUnsigned32 m_tagHigh;
		};
	};
};

ndBodyKinematic::ndContactMap::ndContactMap()
	:dTree<ndContact*, ndContactkey, dContainersFreeListAlloc<ndContact*>>()
	,m_lock()
{
}

ndContact* ndBodyKinematic::ndContactMap::FindContact(const ndBody* const body0, const ndBody* const body1) const
{
	ndContactkey key(body0->GetId(), body1->GetId());
	dScopeSpinLock lock(m_lock);
	dTreeNode* const node = Find(key);
	return node ? node->GetInfo() : nullptr;
}

void ndBodyKinematic::ndContactMap::AttachContact(ndContact* const contact)
{
	ndBody* const body0 = contact->GetBody0();
	ndBody* const body1 = contact->GetBody1();
	ndContactkey key(body0->GetId(), body1->GetId());
	dScopeSpinLock lock(m_lock);
	dAssert(!Find(key));
	//dAssert(!(key == ndContactkey(208, 209)));
	Insert(contact, key);
}

void ndBodyKinematic::ndContactMap::DetachContact(ndContact* const contact)
{
	ndBody* const body0 = contact->GetBody0();
	ndBody* const body1 = contact->GetBody1();
	ndContactkey key(body0->GetId(), body1->GetId());
	dScopeSpinLock lock(m_lock);
	dAssert(Find(key));
	Remove(key);
}

ndBodyKinematic::ndBodyKinematic()
	:ndBody()
	,m_invWorldInertiaMatrix(dGetZeroMatrix())
	,m_shapeInstance(ndDummyCollision::GetNullShape())
	,m_mass(dVector::m_zero)
	,m_invMass(dVector::m_zero)
	,m_gyroAlpha(dVector::m_zero)
	,m_gyroTorque(dVector::m_zero)
	,m_contactList()
	,m_lock()
	,m_scene(nullptr)
	,m_sceneNode(nullptr)
	,m_sceneBodyBodyNode(nullptr)
	,m_sceneAggregateNode(nullptr)
	,m_index(0)
{
	m_invWorldInertiaMatrix[3][3] = dFloat32(1.0f);
	m_shapeInstance.m_ownerBody = this;
}

ndBodyKinematic::~ndBodyKinematic()
{
}

ndShapeInstance& ndBodyKinematic::GetCollisionShape()
{
	return (ndShapeInstance&)m_shapeInstance;
}

const ndShapeInstance& ndBodyKinematic::GetCollisionShape() const
{
	return m_shapeInstance;
}

void ndBodyKinematic::SetCollisionShape(const ndShapeInstance& shapeInstance)
{
	m_shapeInstance = shapeInstance;
	m_shapeInstance.m_ownerBody = this;
}

ndSceneAggregate* ndBodyKinematic::GetBroadPhaseAggregate() const
{
	return m_sceneAggregateNode;
}

void ndBodyKinematic::SetBroadPhaseAggregate(ndSceneAggregate* const node)
{
	m_sceneAggregateNode = node;
}

void ndBodyKinematic::ReleaseMemory()
{
	ndContactMap::FlushFreeList();
}

ndContact* ndBodyKinematic::FindContact(const ndBody* const otherBody) const
{
	return m_contactList.FindContact(this, otherBody);
}

void ndBodyKinematic::AttachContact(ndContact* const contact)
{
	dAssert((this == contact->GetBody0()) || (this == contact->GetBody1()));
	m_contactList.AttachContact(contact);
}

void ndBodyKinematic::DetachContact(ndContact* const contact)
{
	dAssert((this == contact->GetBody0()) || (this == contact->GetBody1()));
	m_contactList.DetachContact(contact);
}

void ndBodyKinematic::SetMassMatrix(dFloat32 mass, const dMatrix& inertia)
{
	mass = dAbs(mass);

	//if (m_collision->IsType(dgCollision::dgCollisionMesh_RTTI) || m_collision->IsType(dgCollision::dgCollisionScene_RTTI)) {
	//	mass = DG_INFINITE_MASS * 2.0f;
	//}

	ndShape* const shape = m_shapeInstance.GetShape();
	if ((mass < D_MINIMUM_MASS) || shape->GetAsShapeNull() || !shape->GetAsShapeConvex())
	{
		mass = D_INFINITE_MASS * 2.0f;
	}

	//if (m_collision->IsType(dgCollision::dgCollisionCompound_RTTI)) 
	//{
	//	const dgCollision* const childShape = m_collision->GetChildShape();
	//	if ((childShape->m_inertia.m_x < dFloat32(1.0e-5f)) || (childShape->m_inertia.m_y < dFloat32(1.0e-5f)) || (childShape->m_inertia.m_z < dFloat32(1.0e-5f))) 
	//	{
	//		mass = DG_INFINITE_MASS * 2.0f;
	//	}
	//}

	//dAssert (m_masterNode);
	//m_world->GetBroadPhase()->CheckStaticDynamic(this, mass);

	if (mass >= D_INFINITE_MASS)
	{
		//if (m_masterNode) {
		//	if (m_invMass.m_w != dFloat32(0.0f)) {
		//		dgBodyMasterList& masterList(*m_world);
		//		if (masterList.GetFirst() != m_masterNode) {
		//			masterList.InsertAfter(masterList.GetFirst(), m_masterNode);
		//		}
		//	}
		//}

		m_mass.m_x = D_INFINITE_MASS;
		m_mass.m_y = D_INFINITE_MASS;
		m_mass.m_z = D_INFINITE_MASS;
		m_mass.m_w = D_INFINITE_MASS;
		m_invMass = dVector::m_zero;
	}
	else
	{
		dFloat32 Ixx = dAbs(inertia[0][0]);
		dFloat32 Iyy = dAbs(inertia[1][1]);
		dFloat32 Izz = dAbs(inertia[2][2]);

		dFloat32 Ixx1 = dClamp(Ixx, dFloat32(0.001f) * mass, dFloat32(1000.0f) * mass);
		dFloat32 Iyy1 = dClamp(Iyy, dFloat32(0.001f) * mass, dFloat32(1000.0f) * mass);
		dFloat32 Izz1 = dClamp(Izz, dFloat32(0.001f) * mass, dFloat32(1000.0f) * mass);

		dAssert(Ixx > dFloat32(0.0f));
		dAssert(Iyy > dFloat32(0.0f));
		dAssert(Izz > dFloat32(0.0f));

		//if (m_masterNode) {
		//	if (m_invMass.m_w == dFloat32(0.0f)) {
		//		dgBodyMasterList& masterList(*m_world);
		//		masterList.RotateToEnd(m_masterNode);
		//	}
		//}

		m_mass.m_x = Ixx1;
		m_mass.m_y = Iyy1;
		m_mass.m_z = Izz1;
		m_mass.m_w = mass;

		m_invMass.m_x = dFloat32(1.0f) / Ixx1;
		m_invMass.m_y = dFloat32(1.0f) / Iyy1;
		m_invMass.m_z = dFloat32(1.0f) / Izz1;
		m_invMass.m_w = dFloat32(1.0f) / mass;
	}

	//#ifdef _DEBUG
#if 0
	dgBodyMasterList& me = *m_world;
	for (dgBodyMasterList::dgListNode* refNode = me.GetFirst(); refNode; refNode = refNode->GetNext()) {
		dgBody* const body0 = refNode->GetInfo().GetBody();
		dVector invMass(body0->GetInvMass());
		if (invMass.m_w != 0.0f) {
			for (; refNode; refNode = refNode->GetNext()) {
				dgBody* const body1 = refNode->GetInfo().GetBody();
				dVector invMass1(body1->GetInvMass());
				dAssert(invMass1.m_w != 0.0f);
			}
			break;
		}
	}
#endif
}

dFloat32 ndBodyKinematic::RayCast(ndRayCastNotify& callback, const dFastRayTest& ray, dFloat32 maxT) const
{
	dVector l0(ray.m_p0);
	dVector l1(ray.m_p0 + ray.m_diff.Scale(dMin(maxT, dFloat32(1.0f))));

	if (dRayBoxClip(l0, l1, m_minAABB, m_maxAABB))
	{
		const dMatrix& globalMatrix = m_shapeInstance.GetGlobalMatrix();
		dVector localP0(globalMatrix.UntransformVector(l0));
		dVector localP1(globalMatrix.UntransformVector(l1));
		dVector p1p0(localP1 - localP0);
		dAssert(p1p0.m_w == dFloat32(0.0f));
		if (p1p0.DotProduct(p1p0).GetScalar() > dFloat32(1.0e-12f))
		{
			ndContactPoint contactOut;
			dFloat32 t = m_shapeInstance.RayCast(callback, localP0, localP1, maxT, this, contactOut);
			if (t < dFloat32(1.0f))
			{
				dAssert(localP0.m_w == dFloat32(0.0f));
				dAssert(localP1.m_w == dFloat32(0.0f));
				dVector p(globalMatrix.TransformVector(localP0 + (localP1 - localP0).Scale(t)));
				t = ray.m_diff.DotProduct(p - ray.m_p0).GetScalar() / ray.m_diff.DotProduct(ray.m_diff).GetScalar();
				if (t < maxT)
				{
					dAssert(t >= dFloat32(0.0f));
					dAssert(t <= dFloat32(1.0f));
					contactOut.m_body0 = this;
					contactOut.m_body1 = this;
					contactOut.m_point = p;
					contactOut.m_normal = globalMatrix.RotateVector(contactOut.m_normal);
					maxT = callback.OnRayCastAction(contactOut, t);
				}
			}
		}
	}
	return maxT;
}

void ndBodyKinematic::UpdateCollisionMatrix()
{
	m_shapeInstance.SetGlobalMatrix(m_shapeInstance.GetLocalMatrix() * m_matrix);
	m_shapeInstance.CalculateFastAABB(m_shapeInstance.GetGlobalMatrix(), m_minAABB, m_maxAABB);
}

dMatrix ndBodyKinematic::CalculateInvInertiaMatrix() const
{
	const dVector invIxx(m_invMass[0]);
	const dVector invIyy(m_invMass[1]);
	const dVector invIzz(m_invMass[2]);
	return dMatrix(
		m_matrix.m_front.Scale(m_matrix.m_front[0]) * invIxx +
		m_matrix.m_up.Scale(m_matrix.m_up[0])		* invIyy +
		m_matrix.m_right.Scale(m_matrix.m_right[0]) * invIzz,

		m_matrix.m_front.Scale(m_matrix.m_front[1]) * invIxx +
		m_matrix.m_up.Scale(m_matrix.m_up[1])		* invIyy +
		m_matrix.m_right.Scale(m_matrix.m_right[1]) * invIzz,

		m_matrix.m_front.Scale(m_matrix.m_front[2]) * invIxx +
		m_matrix.m_up.Scale(m_matrix.m_up[2])		* invIyy +
		m_matrix.m_right.Scale(m_matrix.m_right[2]) * invIzz,
		dVector::m_wOne);
}

dVector ndBodyKinematic::CalculateLinearMomentum() const
{
	return dVector(m_veloc.Scale(m_mass.m_w));
}

dVector ndBodyKinematic::CalculateAngularMomentum() const
{
	dVector localOmega(m_matrix.UnrotateVector(m_omega));
	dVector localAngularMomentum(m_mass * localOmega);
	return m_matrix.RotateVector(localAngularMomentum);
}

void ndBodyKinematic::UpdateGyroData()
{
	if (m_gyroTorqueOn) 
	{
		m_gyroTorque = m_omega.CrossProduct(CalculateAngularMomentum());
		m_gyroAlpha = m_invWorldInertiaMatrix.RotateVector(m_gyroTorque);
	}
	else 
	{
		m_gyroTorque = dVector::m_zero;
		m_gyroAlpha = dVector::m_zero;
	}
}

void ndBodyKinematic::UpdateInvInertiaMatrix()
{
	dAssert(m_invWorldInertiaMatrix[0][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[1][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[2][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[3][3] == dFloat32(1.0f));

	m_invWorldInertiaMatrix = CalculateInvInertiaMatrix();

	dAssert(m_invWorldInertiaMatrix[0][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[1][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[2][3] == dFloat32(0.0f));
	dAssert(m_invWorldInertiaMatrix[3][3] == dFloat32(1.0f));

	UpdateGyroData();
}