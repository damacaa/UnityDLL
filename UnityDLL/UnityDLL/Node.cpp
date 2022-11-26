#include "pch.h"
#include "Node.h"
#include "Collider.h"

void Node::Initialize(int ind)
{
	index = ind;
}

void Node::GetPosition(Eigen::VectorXd* pos)
{
	pos->coeffRef(index) = position[0];
	pos->coeffRef(index + 1) = position[1];
	pos->coeffRef(index + 2) = position[2];
}

void Node::SetPosition(Eigen::VectorXd* pos)
{
	position = Eigen::Vector3d((*pos)[index], (*pos)[index + 1], (*pos)[index + 2]);
}

void Node::GetVelocity(Eigen::VectorXd* _vel)
{
	(*_vel)[index] = vel[0];
	(*_vel)[index + 1] = vel[1];
	(*_vel)[index + 2] = vel[2];
}

void Node::SetVelocity(Eigen::VectorXd* _vel)
{
	vel = Eigen::Vector3d((*_vel)[index], (*_vel)[index + 1], (*_vel)[index + 2]);
}

void Node::GetMass(std::vector<T>* massTripletVector)
{
	(*massTripletVector).push_back(T(index, index, mass));
	(*massTripletVector).push_back(T(index + 1, index + 1, mass));
	(*massTripletVector).push_back(T(index + 2, index + 2, mass));
}

void Node::GetMassInv(std::vector<T>* massTripletVector)
{
	(*massTripletVector).push_back(T(index, index, 1.0 / mass));
	(*massTripletVector).push_back(T(index + 1, index + 1, 1.0 / mass));
	(*massTripletVector).push_back(T(index + 2, index + 2, 1.0 / mass));
}

void Node::GetForce(Eigen::VectorXd* force, std::vector<Collider*> colliders)
{
	Eigen::Vector3d forceV = Eigen::Vector3d(0, -mass * 9.81, 0);
	forceV += -damping * vel;

	(*force)[index] = forceV.x();
	(*force)[index + 1] = forceV.y();
	(*force)[index + 2] = forceV.z();

	for (Collider* collider : colliders)
	{
		Eigen::Vector3d f = collider->GetForce(position);
		(*force)[index] += f.x();
		(*force)[index + 1] += f.y();
		(*force)[index + 2] += f.z();
	}
}

void Node::GetForceJacobian(std::vector<T>* derivPos, std::vector<T>* derivVel, std::vector<Collider*> colliders)
{
	//Needs fix!
	for (Collider* collider : colliders)
	{
		Eigen::Matrix3d dfdx = collider->GetJacobian(position);

		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				if (dfdx(i, j) != 0)
					derivPos->push_back(T(index + i, index + j, dfdx(i, j)));
			}
		}
	}

	derivVel->push_back(T(index, index, -damping));
	derivVel->push_back(T(index + 1, index + 1, -damping));
	derivVel->push_back(T(index + 2, index + 2, -damping));
}

void Node::SetMass(double value)
{
	//density = value;
	//mass = volume * density;
	mass = value;
}

void Node::SetDamping(float alpha)
{
	damping = alpha * mass;
}

