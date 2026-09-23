#include "pch.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <format>
#include <wchar.h>
#include <cstdio>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include "TrafficSim.h"

using json = nlohmann::json;
using std::string;
using std::vector;


enum class NodeType {
	Core,
	Distribution,
	Access,
	Subscriber
};

std::string ip_to_string(uint32_t ip) {
	unsigned char b[4];
	b[0] = ip & 0xFF;
	b[1] = (ip >> 8) & 0xFF;
	b[2] = (ip >> 16) & 0xFF;
	b[3] = (ip >> 24) & 0xFF;
	return std::format("{}.{}.{}.{}", b[3], b[2], b[1], b[0]);
}

// строка "10.0.0.1" -> ip (0x0A000001)
uint32_t string_to_ip(const std::string& s) {
	unsigned int a, b, c, d;
	// sscanf парсит строку по формату
	if (sscanf_s(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
		throw std::invalid_argument("Неверный формат IP: " + s);
	}
	return (a << 24) | (b << 16) | (c << 8) | d;
}

void print_ip(uint32_t ip) { //вывод из 0xFFFFFFFF в формат 255.255.255.255
	std::cout << ip_to_string(ip);
}

//parent_ip сделать чтобы сразу присваивался при создании объекта, если создаётся не Core
class Node {
public:
	int capacity_mbps;
	uint32_t ip;
	uint32_t parent_ip;
	NodeType type;
	vector<uint32_t> children_ips;
	float max_load_mbps = 0.0f;       // максимум за всю симуляцию (для статистики)
	float demand_mbps = 0.0f; //Запрос трафика до урезания при перегрузке
	float current_load_mbps = 0.0f; //Реальный трафик при условии что могут урезать

	Node(NodeType _type, uint32_t _ip, int _capacity_mbps) {
		capacity_mbps = _capacity_mbps;
		ip = _ip;
		parent_ip = 0x00000000;
		type = _type;
		children_ips = {};
	}
	Node(NodeType _type, uint32_t _ip) {
		capacity_mbps = 0;
		ip = _ip;
		parent_ip = 0x00000000;
		type = _type;
		children_ips = {};
	}

	Node(NodeType _type) {
		capacity_mbps = 0;
		ip = 0x00000000;
		parent_ip = 0x00000000;
		type = _type;
		children_ips = {};
	}
	Node() {
		capacity_mbps = 0;
		ip = 0x00000000;
		parent_ip = 0x00000000;
		type = NodeType::Subscriber;
		children_ips = {};
	}
	//Линк дочернего узла
	void node_link(Node& child) {
		children_ips.push_back(child.ip);
		child.parent_ip = ip; //Присвоение ребенку родительского ip
	}
	void print_children_ips() {
		std::cout << "Children ip of ";
		print_ip(ip);
		std::cout << ":\n";
		for (int i = 0; i < children_ips.size(); i++) {
			print_ip(children_ips[i]);
			std::cout << "\n";
		}
	}
	void print_node_type() {
		switch (type) {
		case NodeType::Core:
			std::cout << "Core";
			break;
		case NodeType::Distribution:
			std::cout << "Distribution";
			break;
		case NodeType::Access:
			std::cout << "Access";
			break;
		case NodeType::Subscriber:
			std::cout << "Subscriber";
			break;
		}
	}
	void print_properties() {
		std::cout << capacity_mbps << "\n";
		print_node_type();
		print_ip(ip);
	}

	//Генерирует нагрузку на текущий час
	float simulate_hour(int hour, std::mt19937& rng) {
		if (type != NodeType::Subscriber) return 0.0f;
		if (capacity_mbps <= 0) return 0.0f;

		current_load_mbps = generate_hourly_load(hour, capacity_mbps, rng);
		if (current_load_mbps > max_load_mbps) {
			max_load_mbps = current_load_mbps;
		}
		return current_load_mbps;
	}
	//json для сохранения/загрузки узлов
	json toJson() const {
		json j;
		j["ip"] = ip_to_string(ip);
		j["parent_ip"] = ip_to_string(parent_ip);
		j["capacity_mbps"] = capacity_mbps;
		j["type"] = (int)type;
		j["max_load_mbps"] = max_load_mbps;
		json children = json::array();
		for (uint32_t c : children_ips) {
			children.push_back(ip_to_string(c));
		}
		j["children_ips"] = children;
		return j;
	}
	void fromJson(const json& j) {
		ip = string_to_ip(j["ip"]);
		parent_ip = string_to_ip(j["parent_ip"]);
		capacity_mbps = j["capacity_mbps"];
		type = (NodeType)j["type"];
		max_load_mbps = j.value("max_load_mbps", 0.0f);
		children_ips.clear();
		for (auto& child_str : j["children_ips"]) {
			children_ips.push_back(string_to_ip(child_str));
		}
	}
};

class Network {
private:
	std::unordered_map<uint32_t, Node> nodes;
	std::string filename = "network_state.json";
	int current_hour = 0;
	int current_day = 0;
public:
	int get_current_hour() const { return current_hour; }
	int get_current_day() const { return current_day; }
	void set_filename(const std::string& fname) {
		filename = fname;
	}
	void save_to_file() {
		json j;
		for (auto& pair : nodes) {
			j[ip_to_string(pair.first)] = pair.second.toJson(); //ip - ключ, Нода.toJson() - значение
		}
		std::ofstream file(filename);
		file << j.dump(4);
		file.close();
		std::cout << "Graph saved to " << filename << "\n";
	}
	void load_from_file() {
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cout << "File not found, creating new one..." << "\n";
			return;
		}

		json j;
		file >> j;
		file.close();
		nodes.clear();

		for (auto& [key, value] : j.items()) {
			uint32_t ip = string_to_ip(key);
			Node node;
			node.fromJson(value);
			nodes[ip] = node;
		}
		std::cout << "Loaded " << nodes.size() << " nodes" << "\n";
	}
	//Автоматическая загрузка при создании
	Network() {
		
	}

	bool add_node(NodeType type, uint32_t ip, int capacity_mbps) {
		if (nodes.find(ip) != nodes.end()) {
			std::cout << "IP already exists: ";
			print_ip(ip);
			std::cout << "\n";
			return false;
		}
		nodes[ip] = Node(type, ip, capacity_mbps);
		std::cout << "Node ";
		print_ip(ip);
		std::cout << " added\n";
		return true;
	}

	Node* get_node(uint32_t ip) {
		auto it = nodes.find(ip);
		if (it != nodes.end()) {
			return &(it->second);
		}
		return nullptr;
	}

	bool link_ip(uint32_t parent_ip, uint32_t child_ip) {
		Node* parent = get_node(parent_ip);
		Node* child = get_node(child_ip);

		if (!parent) {
			std::cout << "Parent not found: ";
			print_ip(parent_ip);
			return false;
		}
		if (!child) {
			std::cout << "Child not found: ";
			print_ip(child_ip);
			return false;
		}

		parent->node_link(*child);
		return true;
	}

	//Обёртки под строки
	Node* get_node_str(const std::string& ip_str) {
		return get_node(string_to_ip(ip_str));
	}
	bool link_ip_str(const std::string& parent_ip_str, const std::string& child_ip_str) {
		return link_ip(string_to_ip(parent_ip_str), string_to_ip(child_ip_str));
	}
	bool add_node_str(NodeType type, const std::string& ip_str, int capacity) {
		return add_node(type, string_to_ip(ip_str), capacity);
	}
	void remove_node(uint32_t ip) {
		nodes.erase(ip);
	}
	void print_topology() {
		std::cout << "printing topology" << "\n";
		bool found = false;
		for (auto& pair : nodes) {
			if (pair.second.type == NodeType::Core) {
				print_subtree(pair.first, 0);
				found = true;
			}
		}
		if (!found) {
			std::cout << "Ядро не найдено в топологии\n";
		}
	}
	void apply_fair_share(Node& node) { //Урезание трафика при перегрузке
		if (node.demand_mbps <= node.capacity_mbps) {
			node.current_load_mbps = node.demand_mbps;
			return;
		}

		//Коэффициент урезания
		float k = (float)node.capacity_mbps / node.demand_mbps;
		node.current_load_mbps = (float)node.capacity_mbps;

		//Пропорционально урезаем детей
		for (uint32_t child_ip : node.children_ips) {
			Node* child = get_node(child_ip);
			if (child) {
				child->current_load_mbps = child->demand_mbps * k;
			}
		}
	}
	void collect_subtree(uint32_t root_ip, std::vector<uint32_t>& out) {//Все поддеревья выбраного узла
		out.push_back(root_ip);
		Node* node = get_node(root_ip);
		if (!node) return;
		for (uint32_t child_ip : node->children_ips) {
			collect_subtree(child_ip, out);
		}
	}
	void tick(std::mt19937& rng) {
		//Генерация у абонентов
		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Subscriber) {
				node.simulate_hour(current_hour, rng);
				node.demand_mbps = node.current_load_mbps;
			}
		}

		for (auto& [ip, node] : nodes) {
			if (node.type != NodeType::Subscriber) {
				node.demand_mbps = 0.0f;
				node.current_load_mbps = 0.0f;
			}
		}

		//Считаем demand снизу вверх (Subscriber → Access → Distribution → Core)
		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Subscriber) {
				Node* parent = get_node(node.parent_ip);
				if (parent) parent->demand_mbps += node.demand_mbps;
			}
		}
		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Access) {
				Node* parent = get_node(node.parent_ip);
				if (parent) parent->demand_mbps += node.demand_mbps;
			}
		}
		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Distribution) {
				Node* parent = get_node(node.parent_ip);
				if (parent) parent->demand_mbps += node.demand_mbps;
			}
		}

		//Урезание и обновление current_load снизу вверх
		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Access) {
				if (node.demand_mbps <= node.capacity_mbps) {
					node.current_load_mbps = node.demand_mbps;
					// Дети уже имеют правильный current (сгенерированный)
				}
				else {
					float k = (float)node.capacity_mbps / node.demand_mbps;
					node.current_load_mbps = (float)node.capacity_mbps;
					for (uint32_t child_ip : node.children_ips) {
						Node* child = get_node(child_ip);
						if (child) child->current_load_mbps = child->demand_mbps * k;
					}
				}
			}
		}

		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Distribution) {
				node.current_load_mbps = 0.0f;
				for (uint32_t child_ip : node.children_ips) {
					Node* child = get_node(child_ip);
					if (child) node.current_load_mbps += child->current_load_mbps;
				}
			}
		}

		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Distribution) {
				if (node.current_load_mbps > node.capacity_mbps) {
					float k = (float)node.capacity_mbps / node.current_load_mbps;
					node.current_load_mbps = (float)node.capacity_mbps;
					// Урезаем Access-детей
					for (uint32_t child_ip : node.children_ips) {
						Node* child = get_node(child_ip);
						if (child) child->current_load_mbps *= k;
					}
				}
			}
		}

		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Core) {
				node.current_load_mbps = 0.0f;
				for (uint32_t child_ip : node.children_ips) {
					Node* child = get_node(child_ip);
					if (child) node.current_load_mbps += child->current_load_mbps;
				}
			}
		}

		for (auto& [ip, node] : nodes) {
			if (node.type == NodeType::Core) {
				if (node.current_load_mbps > node.capacity_mbps) {
					float k = (float)node.capacity_mbps / node.current_load_mbps;
					node.current_load_mbps = (float)node.capacity_mbps;
					for (uint32_t child_ip : node.children_ips) {
						Node* child = get_node(child_ip);
						if (child) child->current_load_mbps *= k;
					}
				}
			}
		}

		//Обновление максимумов
		for (auto& [ip, node] : nodes) {
			if (node.current_load_mbps > node.max_load_mbps) {
				node.max_load_mbps = node.current_load_mbps;
			}
		}

		current_hour++;
		if (current_hour >= 24) {
			current_hour = 0;
			current_day++;
		}
	}
	const std::unordered_map<uint32_t, Node>& get_nodes() const {
		return nodes;
	}
private:
	void print_subtree(uint32_t ip, int depth) {
		Node* node = get_node(ip);
		if (!node) return;

		for (int i = 0; i < depth; i++) std::cout << "  ";
		node->print_node_type();
		std::cout << " ";
		print_ip(node->ip);
		std::cout << "\n";

		for (uint32_t child_ip : node->children_ips) {
			print_subtree(child_ip, depth + 1);
		}
	}
};

static Network* g_network = nullptr;
static std::string g_last_json;   //Держим строку, чтобы Python успел прочитать
static std::mt19937 g_rng(std::random_device{}());

extern "C" {
	__declspec(dllexport) int net_tick() {
		if (!g_network) return -1;
		g_network->tick(g_rng);
		return g_network->get_current_hour();
	}

	__declspec(dllexport) const char* net_get_state_json() {
		if (!g_network) return "{}";

		json j;
		j["hour"] = g_network->get_current_hour();
		j["day"] = g_network->get_current_day();

		json nodes_arr = json::array();
		for (const auto& [ip, node] : g_network->get_nodes()) {
			json n;
			n["ip"] = ip_to_string(ip);
			n["type"] = (int)node.type;
			n["capacity"] = node.capacity_mbps;
			n["current_load"] = node.current_load_mbps;
			n["max_load"] = node.max_load_mbps;
			n["parent_ip"] = node.parent_ip ? ip_to_string(node.parent_ip) : "";
			nodes_arr.push_back(n);
		}
		j["nodes"] = nodes_arr;

		g_last_json = j.dump(); //сохраняем в статическую строку
		return g_last_json.c_str();
	}
	__declspec(dllexport) void net_init_with_path(const char* state_file_path) {
    if (g_network) delete g_network;
    g_network = new Network();
    g_network->set_filename(state_file_path);
    g_network->load_from_file();
	}
	__declspec(dllexport) int net_add_node(int type, const char* ip_str, int capacity, const char* parent_ip_str) {
		if (!g_network) return -1;

		NodeType t = (NodeType)type;

		// 1. Парсим IP — если формат неверный, возвращаем ошибку
		uint32_t ip;
		uint32_t parent_ip = 0;
		try {
			ip = string_to_ip(ip_str);
			if (t != NodeType::Core) {
				parent_ip = string_to_ip(parent_ip_str);
			}
		}
		catch (...) {
			return -3;   // Invalid IP format
		}

		// 2. Проверка на занятость IP
		if (g_network->get_node(ip) != nullptr) {
			return -4;   // IP already exists
		}

		// 3. Проверки для не-Core узлов
		if (t != NodeType::Core) {
			// 3.1. Родитель вообще задан?
			if (parent_ip == 0) {
				return -5;   // Parent IP is empty
			}

			// 3.2. Родитель существует?
			Node* parent = g_network->get_node(parent_ip);
			if (!parent) {
				return -6;   // Parent not found
			}

			// 3.3. Уровень родителя строго выше?
			// Уровни: Core=0, Distribution=1, Access=2, Subscriber=3
			// Родитель должен быть на уровень выше (меньше численно)
			if ((int)parent->type >= (int)t) {
				return -7;   // Invalid hierarchy (parent level must be higher)
			}
		}
		else {
			// Для Core: должен быть только один Core в сети
			for (const auto& [other_ip, other_node] : g_network->get_nodes()) {
				if (other_node.type == NodeType::Core) {
					return -8;   // Core already exists
				}
			}
		}

		// 4. Добавляем узел
		if (!g_network->add_node(t, ip, capacity)) {
			return -1;
		}

		// 5. Привязываем к родителю (если не Core)
		if (t != NodeType::Core) {
			Node* parent = g_network->get_node(parent_ip);
			Node* child = g_network->get_node(ip);
			if (parent && child) {
				parent->node_link(*child);
			}
		}

		return 0;
	}

	__declspec(dllexport) int net_remove_node(const char* ip_str) {
		if (!g_network) return -1;
		uint32_t ip = string_to_ip(ip_str);

		Node* node = g_network->get_node(ip);
		if (!node) return -1;
		if (node->type == NodeType::Core) return -2;

		//Собираем всех потомков (включая сам узел)
		std::vector<uint32_t> subtree;
		g_network->collect_subtree(ip, subtree);

		//Отвязываем корень поддерева от родителя
		Node* parent = g_network->get_node(node->parent_ip);
		if (parent) {
			auto& ch = parent->children_ips;
			ch.erase(std::remove(ch.begin(), ch.end(), ip), ch.end());
		}

		//Удаляем всё поддерево
		for (uint32_t u : subtree) {
			g_network->remove_node(u);
		}

		return 0;
	}

	__declspec(dllexport) int net_save_state(const char* path) {
		if (!g_network) return -1;
		g_network->set_filename(path);
		g_network->save_to_file();
		return 0;
	}
}