/* Copyright (C) 2025 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <cstring>
#include "navigator_model.hpp"

extern "C" {
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/arc.h"
#include "ai5/game.h"
}

NavigatorModel::Node *NavigatorModel::Node::fromArchive(struct archive *ar,
		const struct ai5_game *game)
{
	Node *root = new Node(NavigatorNode::RootNode);

	struct archive_data *file;
	archive_foreach(file, ar) {
		Node *child = new Node(NavigatorNode::FileNode);
		child->node.ar.type = NavigatorNode::NormalFile;
		child->node.ar.file = file;
		child->node.ar.game = game;
		root->appendChild(child);
	}
	return root;
}

NavigatorModel::Node::Node(NavigatorNode::NodeType type)
	: parentNode(nullptr)
{
	node.type = type;
}

NavigatorModel::Node::~Node()
{
	qDeleteAll(children);

	if (node.type == NavigatorNode::FileNode) {
		switch (node.ar.type) {
		case NavigatorNode::NormalFile:
			break;
		}
	}
}

NavigatorModel::Node *NavigatorModel::Node::child(int i)
{
	if (i < 0 || i >= children.size())
		return nullptr;
	return children[i];
}

NavigatorModel::Node *NavigatorModel::Node::parent()
{
	return parentNode;
}

void NavigatorModel::Node::appendChild(NavigatorModel::Node *child)
{
	children.append(child);
	child->parentNode = this;
}

int NavigatorModel::Node::row()
{
	if (parentNode)
		return parentNode->children.indexOf(const_cast<Node*>(this));
	return 0;
}

int NavigatorModel::Node::childCount()
{
	return children.size();
}

int NavigatorModel::Node::columnCount()
{
	return 3;
}

QVariant NavigatorModel::Node::data(int column) const
{
	if (column == 0) {
		return node.getName();
	} else if (column == 1) {
		return node.getType();
	} else if (column == 2) {
		return node.getValue();
	}
	return QVariant();
}

NavigatorModel *NavigatorModel::fromArchive(std::shared_ptr<struct archive> ar, 
		const struct ai5_game *game, QObject *parent)
{
	NavigatorModel *model = new NavigatorModel(parent);
	model->root = Node::fromArchive(ar.get(), game);
	model->arFile = ar;
	model->game = game;
	return model;
}

NavigatorModel::~NavigatorModel()
{
	delete root;
}

QModelIndex NavigatorModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	Node *parentNode;
	if (!parent.isValid())
		parentNode = root;
	else
		parentNode = static_cast<Node*>(parent.internalPointer());

	Node *childNode = parentNode->child(row);
	if (childNode)
		return createIndex(row, column, childNode);
	return QModelIndex();
}

QModelIndex NavigatorModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	Node *childNode = static_cast<Node*>(index.internalPointer());
	Node *parentNode = childNode->parent();

	if (!parentNode || parentNode == root)
		return QModelIndex();

	return createIndex(parentNode->row(), 0, parentNode);
}

int NavigatorModel::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	Node *parentNode;
	if (!parent.isValid())
		parentNode = root;
	else
		parentNode = static_cast<Node*>(parent.internalPointer());

	return parentNode->childCount();
}

int NavigatorModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return static_cast<Node*>(parent.internalPointer())->columnCount();
	return root->columnCount();
}

QVariant NavigatorModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	Node *node = static_cast<Node*>(index.internalPointer());
	return node->data(index.column());
}

Qt::ItemFlags NavigatorModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	return QAbstractItemModel::flags(index);
}

QVariant NavigatorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return root->data(section);
	return QVariant();
}

NavigatorNode *NavigatorModel::getNode(const QModelIndex &index) const
{
	if (!index.isValid())
		return nullptr;
	return &static_cast<Node*>(index.internalPointer())->node;
}
