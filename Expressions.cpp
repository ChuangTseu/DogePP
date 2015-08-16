#include "Expressions.h"

#include <map>

std::string OperatorExpression::GetStringExpression() const
{
	return "( " + m_left->GetStringExpression() + " " + LUT_ETokenType_OperatorChar[EASINT(m_eOperator)] + " " + m_right->GetStringExpression() + " )";
}

std::string CallExpression::GetStringExpression() const
{
	std::string strArgsList;
	for (const std::unique_ptr<const Expression>& argExp : m_args)
	{
		strArgsList += argExp->GetStringExpression();
		if (argExp != m_args.back())
		{
			strArgsList += ", ";
		}
	}

	return "(" + strArgsList + ")";
}

std::string ConditionalExpression::GetStringExpression() const
{
	return "( " + m_condition->GetStringExpression() + " ? " + m_thenArm->GetStringExpression() + " : " + m_elseArm->GetStringExpression() + " )";
}

std::string NumberExpression::GetStringExpression() const
{
	return m_name;
}

std::string PostfixExpression::GetStringExpression() const
{
	return "( " + m_left->GetStringExpression() + LUT_ETokenType_OperatorChar[EASINT(m_eOperator)] + " )";
}

std::string PrefixExpression::GetStringExpression() const
{
	return "( " + (LUT_ETokenType_OperatorChar[EASINT(m_eOperator)] + m_right->GetStringExpression()) + " )";
}

template <class K, class T>
T StdMap_MandatoryGet(const std::map<K, T>& stdMap, const K& key)
{
	const auto& it = stdMap.find(key);
	DOGE_ASSERT(it != stdMap.cend());
	return it->second;
}

inline int powint(int a, int b) {
	int base = a, res = 1;
	for (int i = 0; i < b; ++i) res *= base;
	return res;
}

typedef int(*intBinaryFunction)(int, int);

int dogeBinaryFn_plus(int a, int b) { return a + b; }
int dogeBinaryFn_minus(int a, int b) { return a - b; }
int dogeBinaryFn_multiplies(int a, int b) { return a * b; }
int dogeBinaryFn_divides(int a, int b) { return a / b; }
int dogeBinaryFn_modulo(int a, int b) { return a % b; }

int dogeBinaryFn_multiplies_self_n_times(int a, int b) { return powint(a, b); }

int dogeBinaryFn_boolean_and(int a, int b) { return a && b; }
int dogeBinaryFn_boolean_or(int a, int b) { return a || b; }
int dogeBinaryFn_boolean_eq(int a, int b) { return a == b; }
int dogeBinaryFn_boolean_noteq(int a, int b) { return a != b; }
int dogeBinaryFn_boolean_gt(int a, int b) { return a > b; }
int dogeBinaryFn_boolean_ge(int a, int b) { return a >= b; }
int dogeBinaryFn_boolean_lt(int a, int b) { return a < b; }
int dogeBinaryFn_boolean_le(int a, int b) { return a <= b; }

int dogeBinaryFn_binary_and(int a, int b) { return a & b; }
int dogeBinaryFn_binary_or(int a, int b) { return a | b; }
int dogeBinaryFn_binary_xor(int a, int b) { return a ^ b; }
int dogeBinaryFn_binary_lshift(int a, int b) { return a << b; }
int dogeBinaryFn_binary_rshift(int a, int b) { return a >> b; }

static const std::map<ETokenType, intBinaryFunction> intBinaryOpsMap = {
	std::make_pair(ETokenType::PLUS, dogeBinaryFn_plus),
	std::make_pair(ETokenType::MINUS, dogeBinaryFn_minus),
	std::make_pair(ETokenType::MUL, dogeBinaryFn_multiplies),
	std::make_pair(ETokenType::DIV, dogeBinaryFn_divides),
	std::make_pair(ETokenType::MOD, dogeBinaryFn_modulo),
	std::make_pair(ETokenType::BIT_NOT, dogeBinaryFn_binary_xor),
	std::make_pair(ETokenType::BIT_AND, dogeBinaryFn_binary_and),
	std::make_pair(ETokenType::BIT_OR, dogeBinaryFn_binary_or),
	std::make_pair(ETokenType::BIT_XOR, dogeBinaryFn_binary_xor),
	std::make_pair(ETokenType::GT, dogeBinaryFn_boolean_gt),
	std::make_pair(ETokenType::LT, dogeBinaryFn_boolean_lt),

	std::make_pair(ETokenType::NEQ, dogeBinaryFn_boolean_noteq),
	std::make_pair(ETokenType::EQ, dogeBinaryFn_boolean_eq),
	std::make_pair(ETokenType::GE, dogeBinaryFn_boolean_ge),
	std::make_pair(ETokenType::LE, dogeBinaryFn_boolean_le),
	std::make_pair(ETokenType::AND, dogeBinaryFn_boolean_and),
	std::make_pair(ETokenType::OR, dogeBinaryFn_boolean_or),
	std::make_pair(ETokenType::BIT_LSHIFT, dogeBinaryFn_binary_lshift),
	std::make_pair(ETokenType::BIT_RSHIFT, dogeBinaryFn_binary_rshift)
};

typedef int(*intUnaryFunction)(int);

int dogeUnaryPrefixFn_plus(int a) { return +a; }
int dogeUnaryPrefixFn_minus(int a) { return -a; }

int dogeUnaryPrefixFn_boolean_negate(int a) { return !a; }

int dogeUnaryPrefixFn_binary_complement(int a) { return ~a; }

static const std::map<ETokenType, intUnaryFunction> intUnaryPrefixOpsMap = {
	std::make_pair(ETokenType::PLUS, dogeUnaryPrefixFn_plus),
	std::make_pair(ETokenType::MINUS, dogeUnaryPrefixFn_minus),
	std::make_pair(ETokenType::BIT_NOT, dogeUnaryPrefixFn_binary_complement),
	std::make_pair(ETokenType::NOT, dogeUnaryPrefixFn_boolean_negate)
};

int OperatorExpression::Evaluate() const
{
	return StdMap_MandatoryGet(intBinaryOpsMap, m_eOperator)(m_left->Evaluate(), m_right->Evaluate());
}

int CallExpression::Evaluate() const
{
	return 0;
	//return m_args.front()->Evaluate(); // TODO Do this correctly
}

int ConditionalExpression::Evaluate() const
{
	return m_condition->Evaluate() ? m_thenArm->Evaluate() : m_elseArm->Evaluate();
}

int NumberExpression::Evaluate() const
{
	return std::atoi(m_name.c_str());
}

int PostfixExpression::Evaluate() const
{
	DOGE_ASSERT_ALWAYS();
	return -1;
}

int PrefixExpression::Evaluate() const
{
	return StdMap_MandatoryGet(intUnaryPrefixOpsMap, m_eOperator)(m_right->Evaluate());
}


