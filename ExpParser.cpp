#include "ExpParser.h"

void Parser::Register(ETokenType eTokenType, std::unique_ptr<const InfixParselet> parselet)
{
	m_infixParseletsMap.insert(std::make_pair(eTokenType, std::move(parselet)));
}

void Parser::Register(ETokenType eTokenType, std::unique_ptr<const PrefixParselet> parselet)
{
	m_prefixParseletsMap.insert(std::make_pair(eTokenType, std::move(parselet)));
}

std::unique_ptr<const Expression> Parser::ParseExpression(int precedence)
{
	Token token = Consume();

	const auto itPrefixParselet = m_prefixParseletsMap.find(token.m_type);
	DOGE_ASSERT_MESSAGE(itPrefixParselet != m_prefixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
	const PrefixParselet* prefix = itPrefixParselet->second.get();

	std::unique_ptr<const Expression> left = prefix->Parse(*this, token);

	while (precedence > GetPrecedence()) {
		token = Consume();

		const auto itInfixParselet = m_infixParseletsMap.find(token.m_type);
		DOGE_ASSERT_MESSAGE(itInfixParselet != m_infixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
		const InfixParselet* infix = itInfixParselet->second.get();

		left = infix->Parse(*this, std::move(left), token);
	}

	return left;
}

Token Parser::Consume()
{
	// Make sure we've read the token.
	LookAhead(0);

	Token token = m_readTokens.front();
	m_readTokens.pop_front();
	return token;
}

Token Parser::ConsumeExpected(ETokenType eExpected)
{
	Token token = LookAhead(0);
	DOGE_ASSERT_MESSAGE(token.m_type == eExpected, "Expected token '%s' and found '%s'.\n", EnumToSzName(eExpected), EnumToSzName(token.m_type));

	return Consume();
}

bool Parser::Match(ETokenType eExpected)
{
	Token token = LookAhead(0);
	if (token.m_type != eExpected) {
		return false;
	}

	Consume();
	return true;
}

Token Parser::LookAhead(int distance)
{
	// Read in as many as needed.
	while (distance >= static_cast<int>(m_readTokens.size())) {
		m_readTokens.push_back(*m_tokens++);
	}

	// Get the queued token.
	return m_readTokens.back();
}

int Parser::GetPrecedence()
{
	const auto itInfixParselet = m_infixParseletsMap.find(LookAhead(0).m_type);
	if (itInfixParselet != m_infixParseletsMap.cend())
	{
		const InfixParselet* parselet = itInfixParselet->second.get();
		return parselet->GetPrecedence();
	}

	return LOWEST_PRECEDENCE;
}


ExpParser::ExpParser(TokenCIt tokenIt) : Parser(tokenIt)
{
	Register(ETokenType::NUMBER, std::make_unique<NumberParselet>());
	Register(ETokenType::QUESTION, std::make_unique<ConditionalParselet>());
	Register(ETokenType::LEFT_PAREN, std::make_unique<GroupParselet>());

	// Register the simple operator parselets.

	// Precedence values from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B

	Prefix(ETokenType::PLUS, GetOperatorPrefixPrecedence(ETokenType::PLUS));
	Prefix(ETokenType::MINUS, GetOperatorPrefixPrecedence(ETokenType::MINUS));
	Prefix(ETokenType::BIT_NOT, GetOperatorPrefixPrecedence(ETokenType::BIT_NOT));
	Prefix(ETokenType::NOT, GetOperatorPrefixPrecedence(ETokenType::NOT));

	InfixLeft(ETokenType::MUL, GetOperatorInfixPrecedence(ETokenType::MUL));
	InfixLeft(ETokenType::DIV, GetOperatorInfixPrecedence(ETokenType::DIV));
	InfixLeft(ETokenType::MOD, GetOperatorInfixPrecedence(ETokenType::MOD));

	InfixLeft(ETokenType::PLUS, GetOperatorInfixPrecedence(ETokenType::PLUS));
	InfixLeft(ETokenType::MINUS, GetOperatorInfixPrecedence(ETokenType::MINUS));

	InfixLeft(ETokenType::BIT_LSHIFT, GetOperatorInfixPrecedence(ETokenType::BIT_LSHIFT));
	InfixLeft(ETokenType::BIT_RSHIFT, GetOperatorInfixPrecedence(ETokenType::BIT_RSHIFT));

	InfixLeft(ETokenType::LT, GetOperatorInfixPrecedence(ETokenType::LT));
	InfixLeft(ETokenType::LE, GetOperatorInfixPrecedence(ETokenType::LE));
	InfixLeft(ETokenType::GT, GetOperatorInfixPrecedence(ETokenType::GT));
	InfixLeft(ETokenType::GE, GetOperatorInfixPrecedence(ETokenType::GE));

	InfixLeft(ETokenType::EQ, GetOperatorInfixPrecedence(ETokenType::EQ));
	InfixLeft(ETokenType::NEQ, GetOperatorInfixPrecedence(ETokenType::NEQ));

	InfixLeft(ETokenType::BIT_AND, GetOperatorInfixPrecedence(ETokenType::BIT_AND));

	InfixLeft(ETokenType::BIT_XOR, GetOperatorInfixPrecedence(ETokenType::BIT_XOR));

	InfixLeft(ETokenType::BIT_OR, GetOperatorInfixPrecedence(ETokenType::BIT_OR));

	InfixLeft(ETokenType::AND, GetOperatorInfixPrecedence(ETokenType::AND));

	InfixLeft(ETokenType::OR, GetOperatorInfixPrecedence(ETokenType::OR));
}

ExpParser::~ExpParser()
{
}

void ExpParser::Postfix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<PostfixOperatorParselet>(precedence));
}

void ExpParser::Prefix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<PrefixOperatorParselet>(precedence));
}

void ExpParser::InfixLeft(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<BinaryOperatorParselet>(precedence, false));
}

void ExpParser::InfixRight(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<BinaryOperatorParselet>(precedence, true));
}

static std::string strSpace = " ";

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
	return m_args.front()->Evaluate(); // TODO Do this correctly
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
