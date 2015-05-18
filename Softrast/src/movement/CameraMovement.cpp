#include "CameraMovement.h"

#include <Windows.h>

namespace Internal
{
	struct InternalCameraMovement
	{
		glm::vec3 inputMovement;
		glm::vec2 inputPitchYaw;
	};
};

CameraMovement::CameraMovement ( )
	: m_Movement ( new Internal::InternalCameraMovement )
{

}

glm::vec3 CameraMovement::GetInputMovementVector ( )
{
	return m_Movement->inputMovement;
}

glm::vec2 CameraMovement::GetInputPitchYaw ( )
{
	return m_Movement->inputPitchYaw;
}

void CameraMovement::Update ( )
{
	m_Movement->inputMovement = glm::vec3 ( 0.0f );
	m_Movement->inputPitchYaw = glm::vec2 ( 0.0f );	

	if ( GetActiveWindow ( ) != GetForegroundWindow ( ) )
		return;

	float speed = 5.0f;

	if ( GetAsyncKeyState ( VK_CONTROL ) )
		speed /= 4.0f;

	if ( GetAsyncKeyState ( VK_LSHIFT ) )
		speed *= 4.0f;

	if ( GetAsyncKeyState ( 'W' ) )
		m_Movement->inputMovement.z -= speed;
	if ( GetAsyncKeyState ( 'S' ) )
		m_Movement->inputMovement.z += speed;
	if ( GetAsyncKeyState ( 'A' ) )
		m_Movement->inputMovement.x -= speed;
	if ( GetAsyncKeyState ( 'D' ) )
		m_Movement->inputMovement.x += speed;
	if ( GetAsyncKeyState ( 'Q' ) )
		m_Movement->inputMovement.y += speed;
	if ( GetAsyncKeyState ( 'E' ) )
		m_Movement->inputMovement.y -= speed;
	if ( GetAsyncKeyState ( VK_UP ) )
		m_Movement->inputPitchYaw.x += 0.25f;
	if ( GetAsyncKeyState ( VK_DOWN ) )
		m_Movement->inputPitchYaw.x -= 0.25f;
	if ( GetAsyncKeyState ( VK_LEFT ) )
		m_Movement->inputPitchYaw.y += 0.25f;
	if ( GetAsyncKeyState ( VK_RIGHT ) )
		m_Movement->inputPitchYaw.y -= 0.25f;
}