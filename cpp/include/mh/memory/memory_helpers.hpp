#pragma once

#include <memory>

namespace mh
{
	template<typename TClass, typename TMember>
	std::shared_ptr<TMember> shared_ptr_to_member(const std::shared_ptr<TClass>& ptr, TMember TClass::* memberVar)
	{
		return std::shared_ptr<TMember>(ptr, &((*ptr).*memberVar));
	}
}
