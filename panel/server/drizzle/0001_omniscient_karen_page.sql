PRAGMA foreign_keys=OFF;--> statement-breakpoint
CREATE TABLE `__new_logs` (
	`id` text PRIMARY KEY NOT NULL,
	`userId` text NOT NULL,
	`name` text NOT NULL,
	`ip` text NOT NULL,
	`country` text NOT NULL,
	`cookies` integer NOT NULL,
	`passwords` integer NOT NULL,
	`creditcards` integer NOT NULL,
	`cryptocurrencies` integer NOT NULL,
	`created_at` integer NOT NULL,
	FOREIGN KEY (`userId`) REFERENCES `users`(`id`) ON UPDATE no action ON DELETE cascade
);
--> statement-breakpoint
INSERT INTO `__new_logs`("id", "userId", "name", "ip", "country", "cookies", "passwords", "creditcards", "cryptocurrencies", "created_at") SELECT "id", "userId", "name", "ip", "country", "cookies", "passwords", "creditcards", "cryptocurrencies", "created_at" FROM `logs`;--> statement-breakpoint
DROP TABLE `logs`;--> statement-breakpoint
ALTER TABLE `__new_logs` RENAME TO `logs`;--> statement-breakpoint
PRAGMA foreign_keys=ON;