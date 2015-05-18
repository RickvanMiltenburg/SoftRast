#pragma once

#include <glm.hpp>

namespace Internal { struct InternalCameraMovement; };
class CameraMovement
{
public:
	CameraMovement	( );

	glm::vec3	GetInputMovementVector	( );
	glm::vec2	GetInputPitchYaw		( );

	void	Update	( );

private:
	Internal::InternalCameraMovement*	m_Movement;
};