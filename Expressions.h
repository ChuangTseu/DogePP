#pragma once

#include <memory>
#include <vector>

#include "Tokens.h"

class Expression {
public:
	virtual std::string GetStringExpression() const = 0;

	virtual int Evaluate() const = 0;
};

class OperatorExpression : public Expression {
public:
	OperatorExpression(std::unique_ptr<const Expression> left, ETokenType eOperator, std::unique_ptr<const Expression> right) :
		m_left(std::move(left)), m_eOperator(eOperator), m_right(std::move(right))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

	std::unique_ptr<const Expression> m_left;
	ETokenType m_eOperator;
	std::unique_ptr<const Expression> m_right;
};

class CallExpression : public Expression {
public:
	CallExpression(std::unique_ptr<const Expression> function, std::vector<std::unique_ptr<const Expression>> args) :
		m_function(std::move(function)), m_args(std::move(args))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_function;
	std::vector<std::unique_ptr<const Expression>> m_args;
};

class ConditionalExpression : public Expression {
public:
	ConditionalExpression(std::unique_ptr<const Expression> condition, std::unique_ptr<const Expression> thenArm,
		std::unique_ptr<const Expression> elseArm) :
		m_condition(std::move(condition)), m_thenArm(std::move(thenArm)), m_elseArm(std::move(elseArm))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_condition;
	std::unique_ptr<const Expression> m_thenArm;
	std::unique_ptr<const Expression> m_elseArm;
};

class NumberExpression : public Expression {
public:
	NumberExpression(const std::string& name) {
		m_name = name;
	}

	std::string GetName() { return m_name; }

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::string m_name;
};

class PostfixExpression : public Expression {
public:
	PostfixExpression(std::unique_ptr<const Expression> left, ETokenType eOperator) :
		m_left(std::move(left)), m_eOperator(eOperator)
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_left;
	ETokenType  m_eOperator;
};

class PrefixExpression : public Expression {
public:
	PrefixExpression(ETokenType eOperator, std::unique_ptr<const Expression> right) :
		m_eOperator(eOperator), m_right(std::move(right))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	ETokenType m_eOperator;
	std::unique_ptr<const Expression> m_right;
};
