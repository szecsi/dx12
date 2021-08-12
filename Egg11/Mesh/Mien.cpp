#include "stdafx.h"
#include "Mesh/Mien.h"

using namespace Egg11;

unsigned int Mesh::Mien::nextId(0);

const Mesh::Mien Mesh::Mien::invalid;

Mesh::Mien::Mien()
{
	id = nextId;
	nextId++;
}

bool Mesh::Mien::operator<(const Mien& co) const
{
	return id < co.id;
}

bool Mesh::Mien::operator==(const Mien& co) const
{
	return id == co.id;
}