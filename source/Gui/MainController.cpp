﻿#include <iostream>
#include <fstream>

#include "Base/GlobalFactory.h"
#include "Base/ServiceLocator.h"
#include "Base/NumberGenerator.h"

#include "Model/Api/ModelBuilderFacade.h"
#include "Model/Api/SimulationController.h"
#include "Model/Api/SimulationContext.h"
#include "Model/Api/SpaceProperties.h"
#include "Model/Api/SimulationAccess.h"
#include "Model/Api/SimulationParameters.h"
#include "Model/Api/Serializer.h"

#include "MainController.h"
#include "MainView.h"
#include "MainModel.h"
#include "DataManipulator.h"
#include "Notifier.h"

MainController::MainController(QObject * parent)
	: QObject(parent)
{
}

MainController::~MainController()
{
	delete _view;
}

void MainController::init()
{
	_model = new MainModel(this);
	_view = new MainView();

	_view->init(_model, this);

	auto factory = ServiceLocator::getInstance().getService<GlobalFactory>();
	_numberGenerator = factory->buildRandomNumberGenerator();
	_numberGenerator->init(12315312, 0);

	auto facade = ServiceLocator::getInstance().getService<ModelBuilderFacade>();
	auto serializer = facade->buildSerializer();
	SET_CHILD(_serializer, serializer);

	connect(_serializer, &Serializer::serializationFinished, this, &MainController::serializationFinished);

	//default simulation
	NewSimulationConfig config{
		8, { 12, 6 },{ 12 * 33 * 3 , 12 * 17 * 3 },
		facade->buildDefaultSymbolTable(),
		facade->buildDefaultSimulationParameters(),
		20000 * 9
	};
	onNewSimulation(config);
}

void MainController::onRunSimulation(bool run)
{
	_simController->setRun(run);
}

void MainController::onNewSimulation(NewSimulationConfig config)
{
	auto origDataManipulator = _dataManipulator;
	auto origNotifier = _notifier;
	auto origSimController = _simController;

	_model->setSimulationParameters(config.parameters);
	_model->setSymbolTable(config.symbolTable);

	auto facade = ServiceLocator::getInstance().getService<ModelBuilderFacade>();
	_simController = facade->buildSimulationController(config.maxThreads, config.gridSize, config.universeSize, config.symbolTable, config.parameters);

	_dataManipulator = new DataManipulator(this);
	_notifier = new Notifier(this);
	auto descHelper = facade->buildDescriptionHelper(_simController->getContext());
	_simAccess = facade->buildSimulationAccess(_simController->getContext());
	_dataManipulator->init(_notifier, _simAccess, descHelper, _simController->getContext());

	_view->setupEditors(_simController, _dataManipulator, _notifier);

	delete origDataManipulator;
	delete origNotifier;
	delete origSimController;

	addRandomEnergy(config.energy);

	_view->refresh();
}

void MainController::onSaveSimulation(string const& filename)
{
	_serializationOperations.push_back({ SerializationOperation::Type::SaveToFile, filename });
	_serializer->serialize(_simController, _simAccess);
}

void MainController::addRandomEnergy(double amount)
{
	DataChangeDescription desc;
	auto universeSize = _simController->getContext()->getSpaceProperties()->getSize();
	double amountPerCell = _simController->getContext()->getSimulationParameters()->cellMinEnergy;
	for (int i = 0; i < amount; ++i) {
		desc.addNewParticle(ParticleChangeDescription().setPos(QVector2D(_numberGenerator->getRandomInt(universeSize.x), _numberGenerator->getRandomInt(universeSize.y)))
			.setVel(QVector2D(_numberGenerator->getRandomReal()*2.0 - 1.0, _numberGenerator->getRandomReal()*2.0 - 1.0))
			.setEnergy(amountPerCell));
	}
	_simAccess->updateData(desc);
}

void MainController::serializationFinished()
{
	for (SerializationOperation operation : _serializationOperations) {
		if (operation.type == SerializationOperation::Type::SaveToFile) {
			string const& data = _serializer->retrieveSerializedSimulation();
			std::ofstream stream(operation.filename, std::ios_base::out | std::ios_base::binary);
			stream << data;
			stream.close();
		}
	}
	_serializationOperations.clear();
}
