#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace ed {

	class SPIRVParser
	{
	public:
		void Parse(const std::vector<unsigned int>& spv, bool trimFunctionNames = true);

		enum class ValueType
		{
			Void,
			Bool,
			Int,
			Float,
			Vector,
			Matrix,
			Struct,
			Unknown
		};

		struct Variable
		{
			std::string Name;
			ValueType Type, BaseType;
			int TypeComponentCount;
			std::string TypeName;
		};

		struct Function
		{
			int LineStart;
			int LineEnd;
			std::vector<Variable> Arguments;
			std::vector<Variable> Locals;

			Variable ReturnType;
		};

	public:
		std::unordered_map<std::string, Function> m_Functions;
		std::unordered_map<std::string, std::vector<Variable>> m_UserTypes;
		std::vector<Variable> m_Uniforms;
		std::vector<Variable> m_Globals;

		bool m_BarrierUsed;
		int m_LocalSizeX, m_LocalSizeY, m_LocalSizeZ;

		int m_ArithmeticInstCount;
		int m_BitInstCount;
		int m_LogicalInstCount;
		int m_TextureInstCount;
		int m_DerivativeInstCount;
		int m_ControlFlowInstCount;
	};

}
